"""Connects to the base station, records every packet and bridges socket clients to the device."""

from __future__ import annotations

import asyncio
import json
from datetime import datetime, timezone
from typing import Any

from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic
from bleak.backends.device import BLEDevice
from bleak.backends.scanner import AdvertisementData
from bleak.exc import BleakBluetoothNotAvailableError, BleakBluetoothNotAvailableReason, BleakError

from . import cbor
from .jsonfile import JSONArrayFile
from .protocol import (
    FRAGMENT_HEADER_SIZE,
    SERVICE_UUID,
    TOPIC_NAMES,
    TOPIC_UUIDS,
    FragmentReassemblyBuffer,
    full_uuid,
    is_valid_uuid,
    log,
    short_uuid,
    split_into_frames,
)
from .server import Client, SocketServer

SCAN_TIMEOUT = 10.0
RETRY_DELAY = 2.0
GATT_TIMEOUT = 10.0

_DENIED = {
    BleakBluetoothNotAvailableReason.DENIED_BY_USER,
    BleakBluetoothNotAvailableReason.DENIED_BY_SYSTEM,
    BleakBluetoothNotAvailableReason.DENIED_BY_UNKNOWN,
}


def _compact(value: Any) -> str:
    return json.dumps(value, separators=(",", ":"), ensure_ascii=False, allow_nan=False)


def _is_notification(value: bytes) -> bool:
    """Every notification is a fragment with a 4-byte header; read responses on this protocol
    are empty. Lets bleak's CoreBluetooth backend tell them apart while a read is pending."""
    return len(value) >= FRAGMENT_HEADER_SIZE


class Recorder:
    def __init__(
        self,
        output: JSONArrayFile | None,
        server: SocketServer | None,
        name_filter: str | None,
        include_raw: bool,
    ):
        self.output = output
        self.server = server
        self.name_filter = name_filter
        self.include_raw = include_raw
        self._address: str | None = None  # remembered after the first connection
        self._client: BleakClient | None = None
        self._characteristics: dict[str, BleakGATTCharacteristic] = {}
        self._buffers: dict[str, FragmentReassemblyBuffer] = {}
        # One GATT operation at a time: keeps fragments of messages from different clients
        # from interleaving, and bleak's CoreBluetooth backend can't track two concurrent
        # reads of the same characteristic (one of them never completes).
        self._gatt_lock = asyncio.Lock()
        if server:
            server.on_line = self.handle_client_line

    # MARK: - Connection loop

    async def run(self) -> None:
        """Scans, connects, records until disconnected, then starts over; runs until cancelled."""
        while True:
            device = await self._find_device()
            if device is None:
                continue
            disconnected = asyncio.Event()
            loop = asyncio.get_running_loop()
            client = BleakClient(
                device,
                disconnected_callback=lambda _: loop.call_soon_threadsafe(disconnected.set),
                services=[full_uuid(SERVICE_UUID)],
            )
            try:
                await client.connect()
                self._address = device.address
                self._client = client
                await self._setup(client)
                await disconnected.wait()
                log.warning("Disconnected - reconnecting")
            except BleakBluetoothNotAvailableError:
                raise
            except (BleakError, OSError, asyncio.TimeoutError) as e:
                log.warning("Connection failed: %s - retrying", e or type(e).__name__)
                await asyncio.sleep(RETRY_DELAY)
            finally:
                self._client = None
                self._characteristics.clear()
                self._buffers.clear()
                if client.is_connected:
                    await client.disconnect()

    async def _find_device(self) -> BLEDevice | None:
        target = f"device {self._address}" if self._address else f"devices advertising service {SERVICE_UUID}"
        if self.name_filter and not self._address:
            target += f' with name containing "{self.name_filter}"'
        log.info("Scanning for %s…", target)

        def matches(device: BLEDevice, adv: AdvertisementData) -> bool:
            if self._address:
                return device.address == self._address
            if self.name_filter:
                name = adv.local_name or device.name or ""
                return self.name_filter.lower() in name.lower()
            return True

        try:
            device = await BleakScanner.find_device_by_filter(
                matches, timeout=SCAN_TIMEOUT, service_uuids=[full_uuid(SERVICE_UUID)]
            )
        except BleakBluetoothNotAvailableError as e:
            if e.reason in _DENIED:
                log.error("Bluetooth permission denied - on macOS allow your terminal under "
                          "System Settings > Privacy & Security > Bluetooth.")
                raise
            if e.reason in (BleakBluetoothNotAvailableReason.NO_BLUETOOTH,
                            BleakBluetoothNotAvailableReason.NO_BLE_CENTRAL_ROLE):
                log.error("No usable Bluetooth LE adapter found.")
                raise
            log.warning("Bluetooth not available (%s) - waiting", e.reason.name)
            await asyncio.sleep(RETRY_DELAY)
            return None
        except (BleakError, OSError) as e:
            log.warning("Scan failed: %s - retrying", e)
            await asyncio.sleep(RETRY_DELAY)
            return None
        if device:
            log.info('Found "%s" (%s) - connecting', device.name or "Unknown Device", device.address)
        return device

    async def _setup(self, client: BleakClient) -> None:
        service = client.services.get_service(full_uuid(SERVICE_UUID))
        if service is None:
            raise BleakError(f"service {SERVICE_UUID} not found on device")
        for char in service.characteristics:
            self._characteristics[short_uuid(char.uuid)] = char
            if "notify" in char.properties or "indicate" in char.properties:
                await client.start_notify(
                    char, self._on_notify, cb={"notification_discriminator": _is_notification}
                )
        # A read is the protocol's "send me the current value now" signal; the reply
        # arrives as a regular fragmented notification.
        for char in service.characteristics:
            if "read" in char.properties:
                await self._request_update(client, char)
        log.info("Subscribed to %d characteristics, recording (Ctrl-C to stop)", len(service.characteristics))

    async def _request_update(self, client: BleakClient, char: BleakGATTCharacteristic) -> None:
        async with self._gatt_lock:
            value = await asyncio.wait_for(client.read_gatt_char(char), GATT_TIMEOUT)
        # Backends that can't tell a read response from a notification may hand us a
        # notification here; don't lose it.
        if _is_notification(value):
            self._on_notify(char, value)

    # MARK: - Receiving

    def _on_notify(self, char: BleakGATTCharacteristic, data: bytearray) -> None:
        uuid = short_uuid(char.uuid)
        buffer = self._buffers.setdefault(uuid, FragmentReassemblyBuffer())
        complete = buffer.ingest(bytes(data))
        if complete is not None:
            self._record(complete, uuid)

    def _record(self, payload: bytes, uuid: str) -> None:
        topic = TOPIC_NAMES.get(uuid, "unknown")
        entry: dict[str, Any] = {
            "receivedAt": datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z"),
            "characteristic": uuid,
            "topic": topic,
        }
        try:
            packet = cbor.decode(payload)
            entry["packet"] = packet
            if self.include_raw:
                entry["raw"] = payload.hex()
            source = packet.get("id", "?") if isinstance(packet, dict) else "?"
            log.info("%s from %s (%d bytes)", topic, source, len(payload))
        except cbor.CBORError as e:
            entry["error"] = str(e)
            entry["raw"] = payload.hex()
            log.warning("%s: failed to decode %d bytes: %s", topic, len(payload), e)

        if self.server:
            self.server.broadcast(_compact(entry))
        if self.output:
            try:
                self.output.append(entry)
            except OSError as e:
                log.error("Failed to write to output file: %s", e)

    # MARK: - Writing

    def _connected_characteristic(self, uuid: str) -> BleakGATTCharacteristic:
        if self._client is None or not self._client.is_connected or not self._characteristics:
            raise BleakError("not connected")
        char = self._characteristics.get(uuid)
        if char is None:
            raise BleakError(f"characteristic {uuid} not found on device")
        return char

    async def write(self, uuid: str, payload: bytes) -> None:
        """Writes `payload` in fragments that fit the ATT MTU, reassembled by the base station
        (BluetoothManager::handleFragmentedWrite), like the iOS app's writeCharacteristic."""
        char = self._connected_characteristic(uuid)
        frames = split_into_frames(payload, char.max_write_without_response_size - FRAGMENT_HEADER_SIZE)
        async with self._gatt_lock:
            for frame in frames:
                # The firmware only declares BLE_GATT_CHR_F_WRITE, so always write with response.
                await asyncio.wait_for(self._client.write_gatt_char(char, frame, response=True), GATT_TIMEOUT)

    # MARK: - Socket clients

    async def handle_client_line(self, line: str, client: Client) -> None:
        """`{"characteristic": "AB31", "packet": {...}}` writes the packet as CBOR; without
        `packet` a fresh value is requested via a GATT read."""
        def reply(**fields: Any) -> None:
            if seq is not None:
                fields["seq"] = seq
            self.server.send(_compact(fields), client)

        seq = None
        try:
            message = json.loads(line)
        except ValueError as e:
            reply(error=f"invalid JSON: {e}")
            return
        if not isinstance(message, dict):
            reply(error="expected a JSON object")
            return
        seq = message.get("seq")

        if isinstance(message.get("characteristic"), str):
            raw_uuid = message["characteristic"]
            if not is_valid_uuid(raw_uuid):
                reply(error=f"invalid characteristic UUID {raw_uuid}")
                return
            uuid = short_uuid(full_uuid(raw_uuid))
        elif message.get("topic") in TOPIC_UUIDS:
            uuid = TOPIC_UUIDS[message["topic"]]
        else:
            reply(error='missing "characteristic" (or a known "topic")')
            return

        try:
            if "packet" not in message:
                await self._request_update(self._client, self._connected_characteristic(uuid))
            else:
                payload = cbor.encode(message["packet"])
                log.info("Writing %s (%d bytes) from %s", TOPIC_NAMES.get(uuid, "unknown"), len(payload), client.label)
                await self.write(uuid, payload)
        except asyncio.TimeoutError:
            reply(error="device did not respond in time")
            return
        except Exception as e:  # report anything else to the client instead of dropping it
            reply(error=str(e) or type(e).__name__)
            return
        reply(ack=uuid)

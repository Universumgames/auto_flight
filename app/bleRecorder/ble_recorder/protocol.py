"""BLE wire contract of the base station.

Mirrors iosApp/iosApp/Models/BluetoothUUIDs.generated.swift (generated from
shared_components/flight_com/ble_protocol.json). Kept as a plain copy so this tool has no
build-time dependency on the generator; unknown characteristics are still recorded by UUID.
"""

from __future__ import annotations

import logging

SERVICE_UUID = "ABF0"
FRAGMENT_HEADER_SIZE = 4

TOPIC_NAMES = {
    "AB10": "sensorUpdate",
    "AB11": "position",
    "AB12": "componentStatus",
    "AB30": "plannedRoute",
    "AB31": "plannedArea",
    "AB32": "routeHistory",
    "AB33": "routeHistoryRequest",
    "AB34": "plannedRouteConfirmation",
    "AB40": "flightData",
    "ABFF": "dataRequest",
}
TOPIC_UUIDS = {name: uuid for uuid, name in TOPIC_NAMES.items()}

_BASE_UUID_SUFFIX = "-0000-1000-8000-00805f9b34fb"

log = logging.getLogger("ble-recorder")


def full_uuid(short_or_full: str) -> str:
    """'AB11' -> '0000ab11-0000-1000-8000-00805f9b34fb'; 128-bit UUIDs pass through."""
    s = short_or_full.strip().lower()
    if len(s) == 4:
        return f"0000{s}{_BASE_UUID_SUFFIX}"
    if len(s) == 8:
        return f"{s}{_BASE_UUID_SUFFIX}"
    return s


def short_uuid(uuid: str) -> str:
    """Inverse of full_uuid: Bluetooth-base UUIDs shrink back to 'AB11', others stay 128-bit."""
    s = uuid.lower()
    if s.endswith(_BASE_UUID_SUFFIX) and s.startswith("0000"):
        return s[4:8].upper()
    return s.upper()


def is_valid_uuid(s: str) -> bool:
    return len(s) in (4, 8, 36) and all(c in "0123456789abcdefABCDEF-" for c in s)


def make_fragment_frame(total_length: int, offset: int, chunk: bytes) -> bytes:
    """One `[0:2) totalLength, [2:4) offset` (little-endian) + chunk frame, like the iOS
    app's BLEFragmentFraming.makeFrame and the base station's handleFragmentedWrite."""
    return total_length.to_bytes(2, "little") + offset.to_bytes(2, "little") + chunk


def split_into_frames(payload: bytes, chunk_size: int) -> list[bytes]:
    if len(payload) > 0xFFFF:
        raise ValueError(f"payload too large ({len(payload)} bytes)")
    if chunk_size <= 0:
        raise ValueError("MTU too small")
    frames = []
    offset = 0
    while True:
        chunk = payload[offset:offset + chunk_size]
        frames.append(make_fragment_frame(len(payload), offset, chunk))
        offset += len(chunk)
        if offset >= len(payload):
            return frames


class FragmentReassemblyBuffer:
    """Same framing as the iOS app's FragmentReassemblyBuffer."""

    def __init__(self):
        self._reset()

    def _reset(self):
        self.data = bytearray()
        self.total_length = 0
        self.received = 0

    def ingest(self, fragment: bytes) -> bytes | None:
        """Feeds one fragment; returns the full payload once complete, else None."""
        if len(fragment) < FRAGMENT_HEADER_SIZE:
            return None
        frag_total = int.from_bytes(fragment[0:2], "little")
        offset = int.from_bytes(fragment[2:4], "little")
        chunk = fragment[FRAGMENT_HEADER_SIZE:]

        if offset == 0:
            self.data = bytearray(frag_total)
            self.total_length = frag_total
            self.received = 0
        elif self.total_length != frag_total or offset > len(self.data):
            log.warning("BLE fragment doesn't match in-progress message; dropping")
            self._reset()
            return None
        if offset + len(chunk) > len(self.data):
            log.warning("BLE fragment overruns declared length; dropping")
            self._reset()
            return None

        self.data[offset:offset + len(chunk)] = chunk
        self.received += len(chunk)
        if self.received < self.total_length:
            return None
        complete = bytes(self.data)
        self._reset()
        return complete

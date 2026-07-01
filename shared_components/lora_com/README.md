# lora_com

ESP-IDF component providing the long-range radio link between the plane (`flight_controller`) and the ground `base_station`. It wraps an SX1262 LoRa module (via [RadioLib](https://github.com/jgromes/RadioLib)) with a small custom protocol on top: message fragmentation/reassembly, acknowledgements with retries, keep-alive pings and connection-state tracking. Consumers just call `sendData()` and register a callback for incoming payloads — the wire protocol is entirely hidden.

Both sides of the link (`flight_controller` and `base_station`) include this same component and must be configured with matching radio parameters to talk to each other.

## Capabilities

- **Singleton API** — `LoRa_Communication` is a global singleton (`LoRa_CommunicationClass::getInstance()`), so there's exactly one radio instance per device.
- **Arbitrary-length send/receive** — `sendData()` transparently splits payloads larger than one LoRa packet (255 bytes minus the internal header) into fragments and reassembles them on the receiving end before your callback ever sees them.
- **Reliable delivery** — every data packet requires an ACK; if none arrives within the timeout, the packet is retransmitted (up to a configurable retry count) before the send is reported as failed.
- **Keep-alive / link status** — a background task sends a `PING` whenever the link has been idle for too long, and updates a shared `ConnectionState` (`CONNECTING` / `CONNECTED`) in `FlightStorage` so the rest of the firmware (and the web UI) can show whether the peer is reachable.
- **Duplicate/echo suppression** — recently-sent packets are tracked so a device doesn't process its own transmission if it happens to hear it echoed back.
- **Signal diagnostics** — `getLastPacketRSSI()` / `getLastPacketSNR()` expose the last packet's signal strength and signal-to-noise ratio.
- **Callback-based receive API** — register any number of `std::function<void(const LoRaPacket&)>` callbacks via `registerReceivePacketCallback()`; they're invoked from a dedicated worker task, not from ISR/receive-task context.
- **Interrupt or poll-driven receive** — if `CONFIG_LORA_DIO0_PIN` is wired up, the receive task blocks on the DIO0 IRQ; otherwise it falls back to polling the radio.
- **AES-128-GCM primitives included** — `lora_encryption.cpp` implements authenticated encrypt/decrypt of payloads, but it is **not currently wired into `sendData()`/the receive path** (no key is ever set) — packets go over the air in plaintext today. Treat this as a building block for future work, not an active feature.

## Usage

```cpp
#include "LoRa_Communication.hpp"

void init() {
    LoRa_Communication.begin(); // starts the radio + background tasks (receive, ping, callback worker, fragment joiner)

    LoRa_Communication.registerReceivePacketCallback([](const LoRaPacket& packet) {
        // packet.payload / packet.length — already reassembled, ACKed, and de-duplicated
        handleIncoming(packet.payload, packet.length);
    });
}

void sendTelemetry(const uint8_t* data, size_t size) {
    LoRa_Communication.sendData(data, size); // fragments, sends, waits for ACK(s), retries on loss
}
```

`begin()` must be called once at startup before sending/receiving. It creates the SPI/GPIO-backed radio, configures it from Kconfig values, and spins up four FreeRTOS tasks:

| Task | Purpose |
|---|---|
| `lora_rx_task` | Waits for incoming packets (DIO0 IRQ or poll), parses headers, handles ACK/PING packets, hands data packets/fragments off to the fragment cache. |
| `lora_ping_task` | Sends a `PING` when the link has been idle longer than `CONFIG_LORA_PING_INTERVAL`; also expires old sent-packet history. |
| `lora_callback_worker` | Drains fully-received packets and invokes the registered callbacks outside of the receive task. |
| `lora_join_fragments` | Reassembles multi-fragment messages once all fragments for a `messageId` have arrived. |

## Wire protocol (internal)

Every over-the-air packet starts with an internal header (`LoRa_Packet_Internal`): a `PacketType` (`HEADER`/`ACK`/`PING`), a `messageId`, and fragmentation info (`fragmentId`/`totalFragments`/`payloadLength`). `sendData()` assigns a new `messageId` per call, splits the payload into as many `HEADER`-type fragments as needed (max ~255 bytes minus the header per fragment), and requires an ACK per fragment before considering the fragment sent.

This is example/thesis-project-scale plumbing, not a general-purpose radio stack — there's no addressing/routing (it's a point-to-point link between exactly two devices) and no encryption in the current send/receive path (see above).

## Configuration

All radio and link parameters are exposed via `idf.py menuconfig` → *LoRa Communication Settings* (backed by [Kconfig](Kconfig)):

| Setting | Default | Notes |
|---|---|---|
| `LORA_FREQUENCY` | 868 MHz | ISM band; must match on both ends. |
| `LORA_TX_POWER` | 17 dBm | 2–17; higher = more range/power draw. |
| `LORA_SPREADING_FACTOR` | 7 | 6–12; higher = more range, lower throughput. Must match on both ends. |
| `LORA_BANDWIDTH` | 125 kHz | Must match on both ends. |
| `LORA_CODING_RATE_DENOMINATOR` | 6 | 4/X error correction; higher = more robust, slower. |
| `LORA_PREAMBLE_LENGTH` | 8 symbols | |
| `LORA_PING_INTERVAL` | 30 s | Send a keep-alive PING if idle this long. |
| `LORA_SYNC_WORD` | 0x34 | Must match on both ends; keep off the public LoRaWAN sync word (`0x12`) for a private link. |
| `LORA_DIO0_PIN` | 14 | IRQ pin; enables interrupt-driven receive instead of polling. |
| `LORA_CS_GPIO` / `LORA_RST_GPIO` / `LORA_MISO_GPIO` / `LORA_MOSI_GPIO` / `LORA_SCK_GPIO` / `LORA_BUSY_GPIO` | 8 / 12 / 11 / 10 / 9 / 13 | SX1262 SPI wiring, board-specific (defaults match the Heltec LoRa32 V3). |

`flight_controller` and `base_station` must use matching frequency, spreading factor, bandwidth and sync word to communicate; TX power, ping interval and pin mapping can differ per device.

## Dependencies

- [`jgromes/radiolib`](../jgromes__radiolib) (`^7.6.0`) — SX1262 driver, pulled in via [idf_component.yml](idf_component.yml).
- [`flight_data`](../flight_data) — `ConnectionState` type and `FlightStorage`/`helper.hpp` (`isDevicePlane()` / `isDeviceBaseStation()`) used to report connection state to the right side of the link.
- ESP-IDF: `driver`, `freertos`, `esp_timer`, `mbedtls` (AES-GCM), `esp_driver_gpio`, `pthread` (see [CMakeLists.txt](CMakeLists.txt)).
- [`EspHal.h`](EspHal.h) provides the RadioLib hardware abstraction layer for ESP32/ESP32-S3; other targets aren't supported.

## Files

| File | Contents |
|---|---|
| [LoRa_Communication.hpp](LoRa_Communication.hpp) / [.cpp](LoRa_Communication.cpp) | Public API, `begin()`, `sendData()`, fragmentation/send-with-retry logic, singleton. |
| [LoRa_ReceiveTask.cpp](LoRa_ReceiveTask.cpp) | Receive task loop, packet parsing, ACK/PING handling, own-packet filtering. |
| [LoRa_PingTask.cpp](LoRa_PingTask.cpp) | Keep-alive ping loop and sent-packet history cleanup. |
| [LoRa_JoinFragmentsTask.cpp](LoRa_JoinFragmentsTask.cpp) | Reassembles complete multi-fragment messages. |
| [LoRa_CallbackWorkerTask.cpp](LoRa_CallbackWorkerTask.cpp) | Dispatches fully-received packets to registered callbacks. |
| [lora_encryption.cpp](lora_encryption.cpp) | AES-128-GCM encrypt/decrypt helpers (see caveat above). |
| [EspHal.h](EspHal.h) | RadioLib HAL implementation for ESP32/ESP32-S3 (SPI + GPIO). |
| [mutex_helper.hpp](mutex_helper.hpp) | `WITH_MUTEX(...)` RAII-style scope-lock macros used throughout the task loops. |
| [Kconfig](Kconfig) | Radio and pin configuration exposed via `idf.py menuconfig`. |

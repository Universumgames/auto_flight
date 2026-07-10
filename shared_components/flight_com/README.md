# flight_com

ESP-IDF component that defines the application-level protocol spoken between the plane (`flight_controller`) and the ground `base_station`. It builds on top of [`lora_com`](../lora_com) (which handles the physical radio link, fragmentation, ACKs/retries) and adds a small set of typed, serializable packets plus the logic to build, send and decode them.

Both sides of the link include this same component; which packets a device is allowed to *send* is controlled at compile time by `FLIGHT_DEVICE_TYPE_PLANE` / `FLIGHT_DEVICE_TYPE_BASE_STATION`, but either side can *decode* any packet type.

## What it does

- Defines a fixed set of packet structs (`packets/*.hpp`), each carrying a `timestamp` and a `PacketType` tag plus its own payload.
- Serializes packets to a flat `uint8_t` buffer (`serialize()`) and hands them to `LoRa_Communication.sendData()`.
- Decodes a raw buffer or `LoRaPacket` back into the right packet struct (`Flight_Communication::decodePacket()`), based on the `PacketType` in the header, with size validation per type.
- Provides `Flight_Communication::send*()` helpers that pull live data from other components (`GPS_Reader`, `Barometer`, `FlightStorage`, `MotorComMaster`) and send the corresponding packet.
- Gates plane-only senders (`sendPlannedRoute`, `sendRouteHistory`, `sendComponentStatus`) and base-station-only senders (`requestRouteHistory`, `sendPlannedArea`) behind `#ifdef FLIGHT_DEVICE_TYPE_PLANE` / `FLIGHT_DEVICE_TYPE_BASE_STATION`.

## Packets

Every packet starts with a `BasePacket` header (`packets/base.hpp`): a `time_t timestamp` and a `PacketType type` byte. Variable-length packets additionally encode a `size_t length` right after the header, followed by that many `Coordinate` elements.

| Packet | Type | Sent by | Direction | Payload |
|---|---|---|---|---|
| `BasePacket` (as `ROUTE_HISTORY_REQUEST`) | `0x33` | base station | base → plane | header only — asks the plane to send its full route history |
| `SensorUpdate` | `0x10` | both | plane ↔ base | `pressure` (hPa, from `Barometer`) |
| `PositionUpdate` | `0x11` | both | plane ↔ base | current `Coordinate` (from `GPS_Reader`) |
| `ComponentStatus` | `0x12` | plane | plane → base | `ConnectionState` of `gps`, `barometer`, `motorControl`, `magnetometer`, `accelerometer` |
| `PlannedRoutePacket` | `0x30` | plane | plane → base | `route`: ordered list of `Coordinate` waypoints the plane computed to cover the planned area |
| `PlannedAreaPacket` | `0x31` | base station | base → plane | `shape`: list of `Coordinate` describing the area the plane should cover |
| `FlightHistoryPacket` | `0x32` | plane | plane → base | `history`: list of `Coordinate` the plane has actually flown since takeoff |

`SensorUpdate`, `PositionUpdate` and `ComponentStatus` are fixed-size and sent periodically by both sides (see `sendUpdateTask()` in the respective controllers). The route/area/history packets are variable-length and event-driven — e.g. `PlannedAreaPacket` is sent whenever the operator edits the area in the base station's web UI, and `PlannedRoutePacket`/`FlightHistoryPacket` are sent by the plane in response.

## Usage

Sending:

```cpp
#include "Flight_Communication.hpp"

Flight_Communication::sendPosition();      // reads GPS_Reader, sends PositionUpdate
Flight_Communication::sendSensorUpdate();  // reads Barometer, sends SensorUpdate

#ifdef FLIGHT_DEVICE_TYPE_BASE_STATION
Flight_Communication::sendPlannedArea(shape); // std::vector<Coordinate>
#endif
```

Receiving — register a callback with `LoRa_Communication` and decode incoming payloads:

```cpp
LoRa_Communication.registerReceivePacketCallback([](const LoRaPacket& packet) {
    auto decoded = Flight_Communication::decodePacket(packet);
    if (!decoded) return; // too short / unknown type / malformed — already logged

    switch (decoded->type) {
    case PacketType::POSITION: {
        auto* pos = reinterpret_cast<PositionUpdate*>(decoded.get());
        FlightStorage.updatePlanePosition(pos->position, pos->timestamp);
        break;
    }
    // ... one case per PacketType you care about
    }
});
```

`decodePacket()` returns a `std::unique_ptr<BasePacket>` (or `nullptr` for an invalid/too-short/unknown packet). The concrete type is known from `decoded->type`, so callers `reinterpret_cast` it to the matching struct (`SensorUpdate`, `PositionUpdate`, `ComponentStatus`, `PlannedRoutePacket`, `PlannedAreaPacket`, `FlightHistoryPacket`) before reading its extra fields. This is how both `FlightController.cpp` and `BaseController.cpp` consume decoded packets — the switch feeds each field straight into `FlightStorage` (e.g. `updatePlanePosition`, `updatePlannedArea`, `updatePlaneBarometerConnectionState`), which is the shared state that drives route planning, the web UI and connection-status display on both ends.

## Dependencies

- [`lora_com`](../lora_com) — the actual radio transport (`LoRa_Communication`, `LoRaPacket`).
- [`flight_data`](../flight_data) — `Coordinate`/`ConnectionState` types and `FlightStorage` (consumed by callers of `decodePacket`, not by this component directly).
- `gps`, `barometer` — read by the `send*()` helpers to fill packet payloads.
- `motor_com_master` (plane build only) — optional dependency, included when `FLIGHT_DEVICE_TYPE_PLANE` is set.
- `nlohmann-json` — private dependency (see [CMakeLists.txt](CMakeLists.txt)).
- ESP-IDF: `driver`, `freertos`, `esp_timer`.

## Files

| File | Contents |
|---|---|
| [Flight_Communication.hpp](Flight_Communication.hpp) / [.cpp](Flight_Communication.cpp) | Public API: `begin()`, `send*()` helpers, `decodePacket()`. |
| [Packets.hpp](Packets.hpp) | Umbrella header pulling in all packet definitions. |
| [packets/base.hpp](packets/base.hpp) | `PacketType` enum, `BasePacket` header struct. |
| [packets/sensor.hpp](packets/sensor.hpp) | `SensorUpdate`. |
| [packets/position.hpp](packets/position.hpp) | `PositionUpdate`. |
| [packets/component.hpp](packets/component.hpp) | `ComponentStatus`. |
| [packets/route.hpp](packets/route.hpp) | `PlannedRoutePacket`. |
| [packets/area.hpp](packets/area.hpp) | `PlannedAreaPacket`. |
| [packets/history.hpp](packets/history.hpp) | `FlightHistoryPacket`. |

# frontend

Web UI / HTTP+WebSocket API component for the base station. It hosts the operator-facing web interface (static files served from LittleFS) and exposes the REST/WebSocket API the UI (or any other client) uses to read live flight/connection/sensor data and to plan a mission area.

## What it does

- **HTTP server**: starts an `esp_http_server` instance (`FrontendHandlerClass::init()`) with a wildcard URI matcher, then registers, in order: ping, WebSocket, REST API handlers, and finally the static file catch-all (must be last, since it matches `/*`).
- **Static file hosting**: mounts a LittleFS partition (`storage`) at `/static` and serves it for any request that doesn't match a more specific handler; `/` maps to `/static/index.html`. Content-Type is guessed from the file extension (html/css/js/png/jpg, else `text/plain`).
- **Live push updates**: a background FreeRTOS task (`sendWSUpdate`) broadcasts flight, connection and sensor packets to all connected WebSocket clients every 5 seconds.
- **WebSocket client tracking**: tracks connected clients by `(httpd_handle_t, socket fd)` pair (not by `httpd_req_t*`, which is only valid for the duration of a single request) so updates can be pushed outside of a request context, and prunes clients whose socket is no longer a valid WebSocket connection.
- Exposed as a singleton (`FrontendHandler`).

## Dependencies

Declared in [`CMakeLists.txt`](CMakeLists.txt) (`REQUIRES` / `PRIV_REQUIRES`) and [`idf_component.yml`](idf_component.yml):

- `flight_data` (`FlightStorage`, shared types like `Coordinate`/`Route`/`ConnectionState`) — the data source for everything served by this component
- `barometer`, `gps` — read directly for the connection-state/altitude packets ([shared_components](../../../shared_components))
- `esp_http_server` — ESP-IDF HTTP/WebSocket server
- `littlefs` (`joltwallet/littlefs`) — filesystem backing the static file storage partition
- `nlohmann-json` (`johboh/nlohmann-json`) — JSON (de)serialization for all API/WebSocket packets

## API

Base path for all endpoints below is the device's IP/hostname (no auth, plain HTTP).

### REST endpoints

| Method | Path | Description |
|---|---|---|
| `GET` | `/api/ping` | Liveness check. Returns the plain text body `pong`. |
| `GET` | `/api/status` | Returns the current `ConnectionUpdatePacket` (see below) as JSON — connection state of base/plane and their GPS, barometer, motor-control and magnetometer links. |
| `POST` | `/api/area` | Body: JSON `AreaDefinePacket` (`{"shape": [{"longitude": ..., "latitude": ...}, ...]}`). Replaces the planned coverage area in `FlightStorage` and returns the number of points received as plain text. |
| `GET` | `/api/area` | Returns the currently planned coverage area as an `AreaDefinePacket` JSON object. |
| `GET` | `/api/route` | Returns the currently planned flight route as a `PlannedRoutePacket` JSON object (`{"type": "plannedRoute", "route": [...]}`). |
| `GET` | `/*` | Static file handler (catch-all, registered last). Serves files from the LittleFS `/static` partition; `/` serves `index.html`. |

### WebSocket

- `GET /api/ws` — upgrade to a WebSocket connection. On handshake, the client is registered to receive periodic push updates (every 5s):
  - a `FlightUpdatePacket` (`type: "flight"`) — base/plane positions, flown route, planned route, plus their last-update timestamps
  - a `ConnectionUpdatePacket` (`type: "connection"`) — same content as `/api/status`
  - a `SensorPacket` (`type: "sensor"`) — base/plane barometric pressure and the calculated altitude difference
  - Sending the text message `"ping"` on the socket triggers a `"pong"` reply.

### Packets (`FrontendPackets.hpp`)

All packets are JSON-serialized via `nlohmann::json` (`NLOHMANN_DEFINE_TYPE_INTRUSIVE`). Packets pushed over the WebSocket carry a `type` discriminator field; ones only used over REST do not.

- `FlightUpdatePacket` (`type: "flight"`) — `basePosition`, `basePositionUpdateTime`, `planePosition`, `planePositionUpdateTime`, `flightRoute`, `flightRouteUpdateTime`, `plannedRoute`, `plannedRouteUpdateTime`
- `ConnectionUpdatePacket` (`type: "connection"`) — `baseConnectionState`, `lastContactBaseStationTimestamp`, `planeConnectionState`, `lastContactPlaneTimestamp`, `gpsConnectionBase`, `gpsConnectionPlane`, `barometerConnectionBase`, `barometerConnectionPlane`, `motorComConnectionPlane`, `magnetometerConnectionPlane` (each `ConnectionState` is `"connecting"` or `"connected"`)
- `SensorPacket` (`type: "sensor"`) — `barometerPressureBase`, `barometerPressurePlane`, `calculatedAltitude`
- `AreaDefinePacket` — `shape`: list of `Coordinate` (`{longitude, latitude}`)
- `PlannedRoutePacket` (`type: "plannedRoute"`) — `route`: list of `Coordinate`
- `BaseUpdatePacket` (`type: "base"`) — defined but currently unused by any handler

## Files

- `Frontend.hpp` / `Frontend.cpp` — the `FrontendHandlerClass` singleton: server init, REST handler registration, WebSocket update packet preparation/broadcast
- `FrontendPackets.hpp` — JSON packet/DTO definitions used by the API and WebSocket
- `websocket.cpp` — WebSocket URI handler, handshake callback, client add/remove/broadcast, async send helpers
- `websocket_helper.hpp` / `websocket_helper.cpp` — shared async-send argument struct and the `"ping"` → `"pong"` responder
- `static_content.cpp` — LittleFS mounting and the static-file catch-all handler
- `CMakeLists.txt`, `idf_component.yml` — ESP-IDF component registration and dependency declarations
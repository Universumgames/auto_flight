# base_controller

Top-level orchestrator component for the base station firmware. It wires together all the other base-station components at startup and contains the main loop that ties incoming plane telemetry (over LoRa) to the local sensors and the web UI's data store.

## What it does

- **Startup/init**: `BaseControllerClass::init()` brings up the I2C bus, flight storage, the web frontend, the LoRa link and the local barometer/GPS, then starts the controller's background task.
- **Local base-station telemetry**: periodically reads the base station's own barometer (and GPS time reference) and pushes pressure/connection-state updates into `FlightStorage`, so the base's altitude/pressure reading is available alongside the plane's.
- **Plane telemetry ingestion**: registers a receive callback on `LoRa_Communication` and decodes incoming packets via `Flight_Communication`, handling:
  - `SENSOR_UPDATE` — plane pressure readings
  - `POSITION` — plane GPS position
  - `PLANNED_ROUTE` — the route the plane is currently flying
  - `COMPONENT_STATUS` — connection state of the plane's GPS, barometer, gyroscope, motor control and magnetometer
  - `ROUTE_HISTORY_REQUEST` — currently a no-op
- **Outbound updates**: subscribes to `FlightStorage` area-change events and sends the newly planned coverage area to the plane via `Flight_Communication::sendPlannedArea`.
- Exposed as a singleton (`BaseController`) so any other component can trigger initialization or rely on it having run.

## Dependencies

Declared in [`CMakeLists.txt`](CMakeLists.txt) (`REQUIRES`) and [`idf_component.yml`](idf_component.yml):

- `barometer`, `gps` — local sensors on the base station ([shared_components](../../../shared_components))
- `lora_com` (`LoRa_Communication`) — long-range link to the plane
- `flight_com` (`Flight_Communication`) — packet encode/decode for the LoRa protocol
- `flight_data` (`FlightStorage`) — shared in-memory store for telemetry/route/connection state, also feeds the web UI
- `i2c_manager` — shared I2C bus used by the local sensors
- `frontend` — hosts the web UI that visualizes the data this component collects
- `littlefs`, `esp_http_server` — required transitively for the frontend's static file serving

## Files

- `BaseController.hpp` / `BaseController.cpp` — the `BaseControllerClass` singleton, init and packet-handling logic
- `CMakeLists.txt`, `idf_component.yml` — ESP-IDF component registration and dependency declarations
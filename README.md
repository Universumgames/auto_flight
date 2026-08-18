# Auto Flight

An autonomous glider that flies pre-planned coverage patterns over an area (e.g. a field), so it can later be extended with a camera for aerial surveying / crop analysis. The plane navigates itself using GPS, an IMU, a magnetometer and a barometer, and stays in contact with a ground base station over a long-range LoRa link. In an emergency the plane can be taken over manually via a standard RC transmitter/receiver.

This is a student project (semester 12, MRB) and is very much a work in progress — see [resources/learning.md](resources/learning.md) for a running log of progress and issues.

## What it does

- **Autonomous waypoint flight**: given a polygon (the area to cover), the flight controller sweeps it into a lawn-mower-pattern route ([route_planner](flight_controller/components/route_planner)) and flies it using PID-based steering (aileron/pitch/thrust) derived from GPS position, gyroscope attitude and magnetometer heading ([FlightController](flight_controller/components/flight_controller/FlightController.cpp)).
- **Sensor fusion for navigation**: GPS (position/altitude), MPU6050 accelerometer/gyroscope (attitude), magnetometer (heading) and a barometer (altitude) all feed the flight controller.
- **Manual override**: an SBUS RC receiver connected to a separate Arduino ("motor controller") can override the autopilot outputs for the control surfaces and throttle at any time.
- **Long-range telemetry**: the plane and the base station talk over LoRa (RadioLib), with a small custom packet/fragmentation protocol ([lora_com](shared_components/lora_com), [flight_com](shared_components/flight_com)) for position, sensor, route and component-status updates, acknowledgements and keep-alive pings.
- **Ground control web UI**: the base station ESP32 hosts a WiFi access point and serves a Vue 3 web app ([frontend_web](frontend_web)) for drawing the target area on a map, previewing the generated route, and monitoring live telemetry/connection status over a WebSocket.

## Repository layout

| Path | What it is |
|---|---|
| [flight_controller/](flight_controller) | ESP-IDF firmware for the plane's main controller (Heltec WiFi LoRa 32 V3 / ESP32-S3). Route planning, sensor fusion, autopilot, LoRa comms. |
| [base_station/](base_station) | ESP-IDF firmware for the ground station (same ESP32-S3 board). Hosts WiFi AP + web UI, relays commands/telemetry to/from the plane over LoRa. |
| [motor_controller/](motor_controller) | PlatformIO/Arduino firmware for an Arduino Nano that reads the SBUS RC receiver and drives the 4 control-surface servos, exposed to the flight controller over I2C. |
| [frontend_web/](frontend_web) | Vue 3 + TypeScript + Leaflet web app served by the base station: area planning, route preview, live map, connection/component status. |
| [shared_components/](shared_components) | ESP-IDF components shared by `flight_controller` and `base_station`: LoRa comms, I2C manager, GPS reader, gyroscope/magnetometer/barometer drivers, flight data storage/serialization. |
| [python/](python) | Small offline helper scripts to visualize the sweep path/route planner output (matplotlib/folium). |
| [doc/](doc), [resources/](resources) | Notes, pinouts, datasheets, BOM ([BOM.md](BOM.md)) and the weekly progress log ([resources/learning.md](resources/learning.md)). |

## Hardware

- 2x Heltec LoRa32 V3 (ESP32-S3 + SX1262 LoRa) — one in the plane (flight controller), one in the base station.
- Arduino Nano (ATmega328) — motor/servo controller, connected via I2C to the flight controller and via SBUS to an RC receiver.
- GPS module (NMEA, parsed with minmea), MPU6050 IMU, magnetometer (HMC5883L/QMC5883P), barometer, SD card (for LittleFS storage on the base station).
- See [BOM.md](BOM.md) and [resources/pinout_lheltec_lora32_v3.png](resources/pinout_lheltec_lora32_v3.png) / [motor_controller/Readme.md](motor_controller/Readme.md) for parts and pinouts.

### Schematics

The plane's and base station's wiring was drawn in [Fritzing](https://fritzing.org/); source files are [assets/schematics_plane.fzz](assets/schematics_plane.fzz) and [assets/schematics_base.fzz](assets/schematics_base.fzz), along with custom Fritzing parts for modules without official support ([BMP280 breakout](<assets/BMP280_Breakout_Board.fzpz>), [GT-U8 GPS module](<assets/GT-U8-GPS-module.fzpz>), [Heltec WiFi Kit 32 (V3)](<assets/Heltec WiFi Kit 32 (V3).fzpz>)).

**Plane:**

![Plane schematic: Heltec LoRa32 V3, GPS, IMU, magnetometer, barometer and the link to the Arduino motor controller](assets/schematics_plane_schem.svg)

![Plane breadboard view showing the wiring](assets/schematics_plane_bb.svg)

**Base station:**

![Base station schematic: Heltec LoRa32 V3, GPS and barometer](assets/schematics_base_schem.svg)

![Base station breadboard view showing the wiring](assets/schematics_base_bb.svg)

## Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) (v5.x) for `flight_controller` and `base_station`. A devcontainer is provided in each (`.devcontainer/`) if you prefer not to install ESP-IDF locally.
- [PlatformIO](https://platformio.org/) for `motor_controller` (Arduino framework), and optionally for `flight_controller` (it also ships a `platformio.ini`).
- [Node.js](https://nodejs.org/) 20.19+/22.12+ and npm for `frontend_web`.
- CMake + a GoogleTest install for running the flight controller's native/host unit tests.
- Python 3 with the packages in [python/requirements.txt](python/requirements.txt) for the route visualization scripts.

## Building & running

### Flight controller (plane)

```sh
cd flight_controller
idf.py set-target esp32s3
idf.py menuconfig   # optional, see Configuration below
idf.py build
idf.py -p <PORT> flash monitor
```

Native/host unit tests (route planner, etc., using GoogleTest, no hardware needed):

```sh
cd flight_controller
cmake -S . -B cmake-build-debug -G Ninja
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug
```

### Base station (ground control)

```sh
cd base_station
idf.py set-target esp32s3
idf.py menuconfig   # optional, see Configuration below
idf.py build
idf.py -p <PORT> flash monitor
```

The base station serves the (built) web UI from LittleFS. Build the frontend first and it will be picked up as the `static` partition image (see `littlefs_create_partition_image` in [base_station/CMakeLists.txt](base_station/CMakeLists.txt)):

```sh
cd frontend_web
npm install
npm run build        # outputs to frontend_web/dist, copied into base_station/static
```

For frontend-only development against a running base station:

```sh
cd frontend_web
npm run dev
```

### Motor controller (servo/SBUS Arduino)

```sh
cd motor_controller
pio run -t upload -e motor_controller
```

Adjust `upload_port` / `monitor_port` in [motor_controller/platformio.ini](motor_controller/platformio.ini) to match your Arduino's serial port.

## Configuration

Most runtime configuration is exposed through ESP-IDF's `idf.py menuconfig` (backed by each component's `Kconfig`), not source edits:

- **LoRa link** ([shared_components/lora_com/Kconfig](shared_components/lora_com/Kconfig)): frequency, TX power, spreading factor, bandwidth, coding rate, sync word, ping interval, and SPI/DIO pin mapping. Both `flight_controller` and `base_station` must use matching radio parameters (frequency, spreading factor, bandwidth, sync word) to talk to each other.
- **GPS** ([shared_components/gps/Kconfig](shared_components/gps/Kconfig)): RX/TX pins and UART number.
- **I2C bus** ([shared_components/i2c_manager/Kconfig](shared_components/i2c_manager/Kconfig)): SDA/SCL pin numbers shared by the IMU, magnetometer, barometer and the motor controller.
- **Motor controller link** ([flight_controller/components/motor_com_master/Kconfig](flight_controller/components/motor_com_master/Kconfig)): I2C address of the Arduino motor controller.
- **Flight behavior** ([flight_controller/components/flight_controller/Kconfig](flight_controller/components/flight_controller/Kconfig)): telemetry send interval, waypoint-reached radius, and rudder servo travel limits.
- **Base station WiFi** ([base_station/components/wifi_helper/Kconfig](base_station/components/wifi_helper/Kconfig)): AP SSID/password, hostname, and an optional "dev mode" to join an existing WiFi network instead of hosting an AP (credentials for dev mode go in `base_station/components/wifi_helper/secrets.h`, which is a local, non-committed file — copy/create it next to `wifi_helper.hpp`).

Each `idf.py menuconfig` run writes its choices to that project's `sdkconfig`. Flight area, mission parameters (max point distance, swath width, overlap) and manual-override triggering are configured at runtime from the web UI / RC transmitter rather than via `Kconfig`.

## Development notes

- `flight_controller` and `base_station` are independent ESP-IDF projects (each with its own `sdkconfig`/`CMakeLists.txt`) that both pull in [shared_components/](shared_components) via `EXTRA_COMPONENT_DIRS`.
- `flight_controller`'s `CMakeLists.txt` switches between a normal ESP-IDF build and a native host build (GoogleTest) depending on whether `ESP_IDF_VERSION` is set in the environment — this is what powers the unit tests above.
- The plane/base-station wire protocol (packet types, fragmentation, acks) lives in [shared_components/lora_com](shared_components/lora_com) and [shared_components/flight_com](shared_components/flight_com); the equivalent TypeScript types for the web UI are under [frontend_web/src/stores](frontend_web/src/stores) and [frontend_web/references/FrontendPackets.hpp](frontend_web/references/FrontendPackets.hpp).

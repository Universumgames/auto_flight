# Base Station

ESP-IDF firmware for the ground station of the [Auto Flight](../README.md) project — an autonomous glider that flies pre-planned coverage patterns over an area. The base station hosts a WiFi access point and a web UI for planning missions and monitoring the plane, and talks to the plane over a long-range LoRa link.

Runs on a Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262 LoRa), the same board as the [flight_controller](../flight_controller).

## What it does

- **WiFi access point + web UI**: brings up a WiFi AP (or joins an existing network in dev mode) and serves the built [frontend_web](../frontend_web) app from a LittleFS partition ([wifi_helper](components/wifi_helper), [frontend](components/frontend)).
- **REST + WebSocket API**: exposes endpoints to define/read the planned coverage area and route, read connection status, and a WebSocket that pushes live flight/connection/sensor updates every 5 seconds ([frontend](components/frontend)).
- **LoRa telemetry link**: receives the plane's position, sensor and route packets and component-status updates over LoRa, and sends the planned coverage area back to the plane ([base_controller](components/base_controller), using the shared [lora_com](../shared_components/lora_com)/[flight_com](../shared_components/flight_com) components).
- **Local sensors**: reads its own barometer and GPS so the base's altitude/pressure/time reference is available alongside the plane's ([base_controller](components/base_controller)).

See the component READMEs for details: [base_controller](components/base_controller/README.md), [frontend](components/frontend/README.md), [wifi_helper](components/wifi_helper/README.md).

## Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) v5.x. A devcontainer is provided (`.devcontainer/`) if you prefer not to install it locally.
- [Node.js](https://nodejs.org/) 20.19+/22.12+ and npm, to build the [frontend_web](../frontend_web) app served by this firmware.

## Building & running

Build and flash the web UI's static files into the `storage` LittleFS partition first, since the firmware serves them:

```sh
cd ../frontend_web
npm install
npm run build        # outputs to frontend_web/dist, picked up as the `static` partition image
```

Then build and flash the firmware:

```sh
cd base_station
idf.py set-target esp32s3
idf.py menuconfig   # optional, see Configuration below
idf.py build
idf.py -p <PORT> flash monitor
```

`idf.py build` (via `littlefs_create_partition_image` in [CMakeLists.txt](CMakeLists.txt)) packages the `static` directory into the `storage` partition and flashes it alongside the app image, so the frontend build above must happen before flashing.

For frontend-only development against a running base station (no reflashing needed for UI changes), run `npm run dev` in `frontend_web` instead and point it at the base station's IP.

## Configuration

Most runtime configuration is exposed through `idf.py menuconfig`, backed by each component's `Kconfig`:

- **WiFi** ([components/wifi_helper/Kconfig](components/wifi_helper/Kconfig)): AP SSID/password (`WIFI_AP_SSID`/`WIFI_AP_PASSWORD`), hostname (`WIFI_DEVICE_HOSTNAME`), and `WIFI_DEV_MODE` to join an existing network instead of hosting an AP. For local dev, credentials can also go in an untracked `components/wifi_helper/secrets.h` (see [wifi_helper README](components/wifi_helper/README.md)) instead of Kconfig.
- **LoRa link** ([shared_components/lora_com/Kconfig](../shared_components/lora_com/Kconfig)): frequency, TX power, spreading factor, bandwidth, coding rate, sync word, ping interval, SPI/DIO pins. Must match the `flight_controller`'s settings for the two to talk to each other.
- **GPS** ([shared_components/gps/Kconfig](../shared_components/gps/Kconfig)): RX/TX pins and UART number.
- **I2C bus** ([shared_components/i2c_manager/Kconfig](../shared_components/i2c_manager/Kconfig)): SDA/SCL pins shared by the local barometer/GPS.

Flight area and mission parameters are configured at runtime from the web UI rather than via Kconfig.

## Repository layout

| Path | What it is |
|---|---|
| [components/base_controller](components/base_controller) | Top-level orchestrator: wires up sensors, LoRa comms and the frontend, and ties plane telemetry to local data. |
| [components/frontend](components/frontend) | HTTP/WebSocket server hosting the web UI and its REST/WebSocket API. |
| [components/wifi_helper](components/wifi_helper) | WiFi AP/station setup. |
| [static/](static) | Built `frontend_web` output, packaged into the `storage` LittleFS partition. |
| [shared_components (../shared_components)](../shared_components) | ESP-IDF components shared with `flight_controller`: LoRa comms, I2C manager, GPS, barometer, magnetometer, flight data storage. |

## Development notes

- `base_station` is an independent ESP-IDF project (its own `sdkconfig`/`CMakeLists.txt`) that pulls in [`../shared_components`](../shared_components) via `EXTRA_COMPONENT_DIRS` in [CMakeLists.txt](CMakeLists.txt).
- `FLIGHT_DEVICE_TYPE_BASE_STATION` is defined project-wide (see [CMakeLists.txt](CMakeLists.txt)) to select base-station-specific behavior in shared components.
- The plane/base-station wire protocol lives in [shared_components/lora_com](../shared_components/lora_com) and [shared_components/flight_com](../shared_components/flight_com); the equivalent TypeScript types for the web UI are under [frontend_web/src/stores](../frontend_web/src/stores).

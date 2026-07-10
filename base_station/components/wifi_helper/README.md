# wifi_helper

Small wrapper around the ESP-IDF WiFi driver for the base station firmware. It brings the WiFi stack up in either Access Point (AP) or Station (STA) mode and exposes the current IP address to the rest of the firmware.

## What it does

- **`init_wifi()`**: entry point used by the rest of the firmware. Starts the AP (`start_ap()`) by default, or connects to an existing network (`connect_wifi()`) when `CONFIG_WIFI_DEV_MODE` is enabled — useful for development so the base station joins your regular WiFi instead of requiring clients to join its AP.
- **`start_ap()`**: brings up NVS/netif/event loop, then starts a WPA2-PSK access point using the SSID/password/hostname from Kconfig.
- **`connect_wifi()`** (only when `CONFIG_WIFI_DEV_MODE` is set): joins an existing network in station mode, auto-retries on disconnect (up to 50 times) and blocks for up to `WIFI_DEV_MODE_CONNECT_TIMEOUT_SEC` seconds (default 30) waiting for a successful connection or final failure. If it can't connect within that time, it tears down the station driver and falls back to `start_ap()` instead.
- **`get_ip_address()`**: returns the current interface's IP address as a string, for display in logs/UI.
- Wifi mode is only initialized once per boot — calling any of the start/connect functions again returns `ESP_FAIL` if a mode is already active.

## Configuration

Configured via `idf.py menuconfig` under **WiFi Helper Configuration** (see [`Kconfig`](Kconfig)):

- `WIFI_AP_SSID` / `WIFI_AP_PASSWORD` — SSID/password used both for the AP and, in dev mode, the network to connect to
- `WIFI_DEV_MODE` — switch from AP mode to station mode for development
- `WIFI_DEV_MODE_CONNECT_TIMEOUT_SEC` — how long (in seconds) dev mode waits to connect to the configured network before giving up and starting the AP instead (default 30)
- `WIFI_DEVICE_HOSTNAME` — hostname advertised on the network

For local development, an optional [`secrets.h`](secrets.h) (untracked, gitignored) can be placed next to the source to override the SSID/password/dev-mode `#define`s without touching Kconfig; [`CMakeLists.txt`](CMakeLists.txt) detects the file and enables it automatically via `HAVE_WIFI_SECRETS_H`.

## Dependencies

Declared in [`CMakeLists.txt`](CMakeLists.txt) (`REQUIRES`):

- `esp_wifi` — ESP-IDF WiFi driver
- `nvs_flash` — non-volatile storage, required by the WiFi driver for calibration data

## Files

- `wifi_helper.hpp` / `wifi_helper.cpp` — public API and implementation
- `secrets.h` — optional, untracked local override for SSID/password/dev-mode
- `CMakeLists.txt`, `Kconfig` — ESP-IDF component registration and configuration menu
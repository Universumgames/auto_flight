# ble-recorder

Command-line counterpart of the iOS app's Bluetooth layer, written in Python with
[bleak](https://github.com/hbldh/bleak), so it runs on macOS, Linux and Windows. It connects
to the first base station advertising service `ABF0`, subscribes to every characteristic,
reassembles the fragmented notifications and appends each decoded packet to a JSON array file.

Packets are decoded generically from CBOR. The base station encodes them as maps keyed by
field name, so the output keeps the real field names without depending on the SharedLogic
framework. The file stays valid JSON after every write.

## Building

Needs Python 3.10+ and `make`. From a fresh clone:

```sh
cd app/bleRecorder
make            # creates .venv, installs bleak + PyInstaller, builds dist/ble-recorder
```

`dist/ble-recorder` (`ble-recorder.exe` on Windows) is a standalone executable with Python
bundled, so you can copy it to a machine without Python. PyInstaller can't cross-compile, so
build on each target OS. The `ble_recorder` CI job builds the Linux executable.

| Command | What it does |
|---|---|
| `make` | Standalone executable in `dist/` |
| `make run` | Run from source (no PyInstaller step), record to `recording-<timestamp>.json` |
| `make run-dist` | Same, using the standalone executable |
| `make test` | Unit tests |
| `make install` | Copy the executable to `$(PREFIX)/bin` (default `/usr/local`) |
| `make clean` / `make distclean` | Remove `build/` + `dist/` / also `.venv` |

`make run` and `make run-dist` take `OUT=`, `NAME=`, `DURATION=`, `RAW=1`, `TCP=`, `SOCKET=`,
`NOFILE=1` and `ARGS="..."` for anything else:

```sh
make run OUT=flight.json DURATION=60      # stop after 60 s
dist/ble-recorder -n base --raw           # filter by name, keep raw CBOR hex
dist/ble-recorder --help
```

## Platform notes

- **macOS:** on first run macOS asks for Bluetooth permission for your terminal app
  (System Settings → Privacy & Security → Bluetooth).
- **Linux:** needs BlueZ, a powered-on adapter (`bluetoothctl power on`) and a user that may
  talk to BlueZ over D-Bus (usually true for desktop users; otherwise add the `bluetooth` group).
- **Windows:** Windows 10+, no pairing needed. Use `make` from Git Bash or MSYS2 (with
  `PYTHON=python` if `python3` isn't on the PATH). Unix sockets aren't available there; use `--tcp`.

Each entry looks like:

```json
{
  "receivedAt": "2026-10-22T14:03:11.512Z",
  "characteristic": "AB10",
  "topic": "sensorUpdate",
  "packet": { "timestamp": 1234, "type": 16, "id": 1, "pressure": 1013.25, ... }
}
```

The UUID → topic name table in `ble_recorder/protocol.py` mirrors
`iosApp/iosApp/Models/BluetoothUUIDs.generated.swift`. Packets on characteristics missing
from that table are still recorded, with `"topic": "unknown"`.

## Socket passthrough

`--tcp [host:]port` (default host `127.0.0.1`) and/or `--socket <path>` let other programs use
the BLE link. Add `--no-file` if you don't also want the JSON file.

```sh
make run TCP=7878 NOFILE=1
make run SOCKET=/tmp/ble-recorder.sock
```

The protocol is newline-delimited JSON, one compact object per line:

- **Device → client:** every received packet is sent to all clients, as the same object
  that goes into the file:
  `{"receivedAt":"…","characteristic":"AB11","topic":"position","packet":{…}}`
- **Client → device:** `{"characteristic":"AB34","packet":{…}}` encodes `packet` to CBOR
  as-is and writes it, fragmented, to that characteristic. `"topic":"plannedRouteConfirmation"`
  works instead of `characteristic`. The packet must contain everything the base station
  expects (`type`, `id`, `timestamp`, …). Integers stay CBOR integers and other numbers are
  sent as float64.
- Leaving out `packet` (`{"characteristic":"AB11"}`) asks the device to send that topic's
  current value, which then arrives as a normal packet.
- **Replies** go only to the sender and have no `packet` key: `{"ack":"AB34"}` once the
  write is acknowledged, or `{"error":"…"}`. An optional `"seq"` is echoed back.

```python
import json, socket

sock = socket.create_connection(("127.0.0.1", 7878))
# sock = socket.socket(socket.AF_UNIX); sock.connect("/tmp/ble-recorder.sock")
sock.sendall(json.dumps({"topic": "position", "seq": 1}).encode() + b"\n")

for line in sock.makefile("rb"):
    msg = json.loads(line)
    if "packet" in msg:
        print(msg["topic"], msg["packet"])
    else:
        print("reply:", msg)
```

Unix socket paths are limited to 103 bytes on macOS.

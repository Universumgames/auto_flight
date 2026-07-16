# Serial Plane Viz

A simple, standalone 3D visualization tool: reads four servo values (motor,
roll, yaw, pitch) from a serial port and animates a low-poly RC plane in the
browser accordingly. Built with plain HTML/JS + three.js, no build step.

## Layout

```
frontend/   Static web app (three.js scene, UI, data source clients)
bridge/     Optional Python serial-to-WebSocket bridge (fallback for
            browsers without Web Serial support)
```

## Wire format

One line per update, comma-separated raw values in the range -100..100
(0 = neutral/center) — the same range `motor_controller` receives over I2C
(as `int8_t`) before its `mapToServo()` converts to actual PWM microseconds:

```
0,62,0,-38\n
```

By default the four fields are read as `yaw, pitch, motor, roll`, in that
order.

## Configuring channel order

The wire order is remapped in exactly one place, `frontend/js/config.js`:

```js
// Reorder this array to match your transmitter/receiver's actual wire order.
// Nothing else in parser.js / plane.js needs to change.
export const CHANNEL_ORDER = ['yaw', 'pitch', 'motor', 'roll'];
```

Edit the array order (and `CHANNEL_TYPE` if a channel's normalization should
change) and reload the page — parsing and rendering adapt automatically.

## Baud rate

The default baud rate (115200) lives in one place per side: `DEFAULT_BAUD_RATE`
in `frontend/js/config.js` (used by Web Serial, editable in the UI too) and
`DEFAULT_BAUD_RATE` in `bridge/config.py` (used by the Python bridge, override
with `--baud`).

The bridge script serves the `frontend/` static files itself (on
`http://localhost:8000` by default), so it's the only thing you need to run
for any bridge-based setup below. Pass `--no-http` if you'd rather serve the
frontend yourself (e.g. `python3 -m http.server`, needed for a Web-Serial-only
setup with no bridge at all).

## Quick start — no hardware (mock demo)

```sh
cd bridge
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python3 serial_to_ws_bridge.py --mock
```

Open `http://localhost:8000`, click **Connect via WS Bridge**, and you
should see the propeller spin and the plane bank, pitch, and yaw smoothly.

## Quick start — real hardware, Web Serial (Chrome/Edge only)

Web Serial requires a secure context; `localhost` qualifies. No Python
bridge needed — just serve the frontend:

```sh
cd frontend
python3 -m http.server 8000
```

Open `http://localhost:8000` in Chrome or Edge, click **Connect via Web
Serial**, and select your device's serial port.

## Quick start — real hardware, Python bridge (any browser)

```sh
cd bridge
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python3 serial_to_ws_bridge.py
```

If `--port` isn't given, the bridge lists detected serial ports and prompts
you to pick one (by number or by typing a custom device path):

```
Available serial ports:
  [0] /dev/tty.usbserial-1420  (USB Serial)
  [1] /dev/tty.Bluetooth-Incoming-Port  (n/a)
Select a port [0-1] or enter a custom path: 0
```

Pass `--port /dev/tty.usbserial-XXXX` directly to skip the prompt.

If the serial connection drops (device unplugged, power cycle, etc.), the
bridge automatically retries the same port every `RECONNECT_DELAY_SECONDS`
(default 2s, in `bridge/config.py`) until it reconnects — no need to restart
the script.

Open `http://localhost:8000`, then click **Connect via WS Bridge**. Use this
path for Firefox/Safari, or any setup where Web Serial isn't available.

## Camera controls

Drag to orbit, scroll to zoom (three.js `OrbitControls`).

## Debug view

Click **Show Debug** to show the latest incoming line, as both raw text and
its parsed channel values (updates in place, no scrollback). A
malformed/dropped line is shown in red with no parsed values — useful for
spotting wiring or baud-rate issues.

## Limitations

The plane body itself is static — only the propeller and the control
surfaces (ailerons, elevator, rudder) move, deflecting in proportion to
their channel value. There's no integrated flight dynamics or whole-plane
attitude change. All angle/speed constants live in one `LIMITS` block at the
top of `frontend/js/plane.js` for easy tuning.

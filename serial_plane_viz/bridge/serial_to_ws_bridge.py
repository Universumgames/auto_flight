#!/usr/bin/env python3
"""Bridge a serial port (CSV servo values) to a local WebSocket for serial_plane_viz.

Also serves the frontend/ static files, so this script alone is enough to run
the whole tool (pass --no-http if you're serving the frontend separately).

Usage:
    python3 serial_to_ws_bridge.py                  # prompts for a serial port to use
    python3 serial_to_ws_bridge.py --port /dev/tty.usbserial-XXXX
    python3 serial_to_ws_bridge.py --mock           # no hardware needed, synthetic demo data
"""
import argparse
import asyncio
import functools
import http.server
import math
import pathlib
import sys
import threading
import time

import websockets

import config

FRONTEND_DIR = pathlib.Path(__file__).resolve().parent.parent / "frontend"

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--port",
        help="Serial port device, e.g. /dev/tty.usbserial-XXXX or COM3. "
        "If omitted, you'll be prompted to pick from detected ports.",
    )
    parser.add_argument(
        "--baud", type=int, default=config.DEFAULT_BAUD_RATE, help="Serial baud rate"
    )
    parser.add_argument(
        "--ws-host", default=config.DEFAULT_WS_HOST, help="WebSocket bind host"
    )
    parser.add_argument(
        "--ws-port", type=int, default=config.DEFAULT_WS_PORT, help="WebSocket bind port"
    )
    parser.add_argument(
        "--mock",
        action="store_true",
        help="Generate synthetic oscillating values instead of reading a real serial port",
    )
    parser.add_argument(
        "--http-host", default=config.DEFAULT_HTTP_HOST, help="Frontend static file server host"
    )
    parser.add_argument(
        "--http-port", type=int, default=config.DEFAULT_HTTP_PORT, help="Frontend static file server port"
    )
    parser.add_argument(
        "--no-http",
        action="store_true",
        help="Don't serve frontend/ (use this if you're already serving it separately)",
    )
    return parser.parse_args()


def start_http_server(host, port, directory):
    """Serves the frontend/ static files in a background thread."""
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(directory))
    httpd = http.server.ThreadingHTTPServer((host, port), handler)
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()
    return httpd


def select_serial_port():
    """Lists detected serial ports and prompts the user to pick one."""
    try:
        ports = list(list_ports.comports())

        if not ports:
            typed = input("No serial ports detected. Enter a device path manually: ").strip()
            if not typed:
                print("error: no serial port provided", file=sys.stderr)
                sys.exit(1)
            return typed

        print("Available serial ports:")
        for i, p in enumerate(ports):
            print(f"  [{i}] {p.device}  ({p.description})")

        while True:
            choice = input(f"Select a port [0-{len(ports) - 1}] or enter a custom path: ").strip()
            if not choice:
                continue
            if choice.isdigit() and 0 <= int(choice) < len(ports):
                return ports[int(choice)].device
            return choice
    except (EOFError, KeyboardInterrupt):
        print("\nerror: no serial port selected", file=sys.stderr)
        sys.exit(1)


async def mock_producer(queue: asyncio.Queue):
    """Generates four independent sine waves in the -100..100 raw value range, at ~20Hz."""
    start = time.monotonic()
    while True:
        t = time.monotonic() - start
        motor = 100 * (0.5 + 0.5 * math.sin(t * 0.6))  # idle -> full throttle, never negative
        roll = 80 * math.sin(t * 0.9)
        yaw = 80 * math.sin(t * 0.5 + 1.5)
        pitch = 80 * math.sin(t * 0.7 + 3.0)
        # Field order must match config.js's CHANNEL_ORDER (yaw, pitch, motor, roll).
        line = f"{yaw:.0f},{pitch:.0f},{motor:.0f},{roll:.0f}\n"
        await queue.put(line)
        await asyncio.sleep(0.05)


def serial_reader_thread(port, baud, queue, loop):
    """Runs in a background thread; pushes decoded lines into the asyncio queue.
    Automatically reconnects to the same port if the connection drops."""
    while True:
        try:
            with serial.Serial(port, baud, timeout=1) as ser:
                print(f"Connected to {port} at {baud} baud")
                while True:
                    raw = ser.readline()
                    if not raw:
                        continue
                    try:
                        line = raw.decode("utf-8", errors="ignore")
                    except UnicodeDecodeError:
                        continue
                    loop.call_soon_threadsafe(queue.put_nowait, line)
        except (serial.SerialException, OSError) as exc:
            print(
                f"Serial connection to {port} lost ({exc}); "
                f"retrying in {config.RECONNECT_DELAY_SECONDS:.0f}s...",
                file=sys.stderr,
            )
            time.sleep(config.RECONNECT_DELAY_SECONDS)


async def broadcaster(queue: asyncio.Queue, clients: set):
    while True:
        line = await queue.get()
        if clients:
            await asyncio.gather(
                *(client.send(line) for client in clients),
                return_exceptions=True,
            )


async def main():
    args = parse_args()

    if not args.mock and serial is None:
        print("error: pyserial is not installed (pip install -r requirements.txt)", file=sys.stderr)
        sys.exit(1)
    if not args.mock and not args.port:
        args.port = select_serial_port()

    if not args.no_http:
        start_http_server(args.http_host, args.http_port, FRONTEND_DIR)
        print(f"Serving frontend at http://{args.http_host}:{args.http_port}")

    queue = asyncio.Queue()
    clients = set()

    async def handler(websocket):
        clients.add(websocket)
        try:
            await websocket.wait_closed()
        finally:
            clients.discard(websocket)

    loop = asyncio.get_running_loop()

    if args.mock:
        asyncio.create_task(mock_producer(queue))
        print("Mock mode: generating synthetic servo values")
    else:
        loop.run_in_executor(None, serial_reader_thread, args.port, args.baud, queue, loop)
        print(f"Bridging serial port {args.port} (auto-reconnects if the connection drops)")

    asyncio.create_task(broadcaster(queue, clients))

    async with websockets.serve(handler, args.ws_host, args.ws_port):
        print(f"WebSocket bridge listening on ws://{args.ws_host}:{args.ws_port}")
        await asyncio.Future()  # run forever


if __name__ == "__main__":
    asyncio.run(main())

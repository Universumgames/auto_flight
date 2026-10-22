from __future__ import annotations

import argparse
import asyncio
import logging
import os
import signal
import sys
import time
from datetime import datetime

from bleak.exc import BleakBluetoothNotAvailableError

from .jsonfile import JSONArrayFile
from .protocol import log
from .bridge import FlightBridge
from .server import SocketServer

DESCRIPTION = """\
Connects to the first base station advertising the auto_flight BLE service, subscribes to
every characteristic and appends each decoded packet to a JSON array file. Optionally serves
the link over a socket as newline-delimited JSON: received packets are pushed to every client,
and {"characteristic": "AB31", "packet": {...}} lines sent by a client are written to the
device as CBOR (see README.md)."""


def parse_endpoint(value: str) -> tuple[str, int]:
    """Parses "7878", "0.0.0.0:7878" or "[::1]:7878"."""
    host, sep, port = value.rpartition(":")
    if not sep:
        host, port = "127.0.0.1", value
    if host.startswith("[") and host.endswith("]"):
        host = host[1:-1]
    try:
        port_number = int(port)
    except ValueError:
        port_number = -1
    if not host or not 0 < port_number < 65536:
        raise argparse.ArgumentTypeError(f"expected [host:]port, got {value!r}")
    return host, port_number


def positive_seconds(value: str) -> float:
    try:
        seconds = float(value)
    except ValueError:
        seconds = -1
    if seconds <= 0:
        raise argparse.ArgumentTypeError("needs a positive number of seconds")
    return seconds


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="ble-flight-bridge", description=DESCRIPTION,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("-o", "--output", metavar="FILE", help="output file (default: recording-<timestamp>.json)")
    p.add_argument("-n", "--name", help="only connect to a device whose name contains NAME")
    p.add_argument("-t", "--duration", type=positive_seconds, metavar="SEC",
                   help="stop after SEC seconds (counted from launch; default: run until Ctrl-C)")
    p.add_argument("--raw", action="store_true", help="also store the raw CBOR payload as hex for each packet")
    p.add_argument("--tcp", type=parse_endpoint, metavar="[HOST:]PORT",
                   help="serve the JSON passthrough over TCP (default host 127.0.0.1)")
    p.add_argument("--socket", metavar="PATH", help="serve the JSON passthrough on a Unix domain socket")
    p.add_argument("--no-file", action="store_true", help="don't write a JSON file (requires --tcp or --socket)")
    p.add_argument("-q", "--quiet", action="store_true", help="only log warnings and errors")
    return p


class _UTCFormatter(logging.Formatter):
    converter = time.gmtime


def setup_logging(quiet: bool) -> None:
    handler = logging.StreamHandler(sys.stderr)
    handler.setFormatter(_UTCFormatter("[%(asctime)s] %(message)s", datefmt="%Y-%m-%dT%H:%M:%SZ"))
    log.addHandler(handler)
    log.setLevel(logging.WARNING if quiet else logging.INFO)
    log.propagate = False


async def run(args: argparse.Namespace) -> int:
    output = None
    if not args.no_file:
        path = args.output or datetime.now().strftime("recording-%Y%m%d-%H%M%S.json")
        try:
            output = JSONArrayFile(path)
        except OSError as e:
            log.error("Cannot create %s: %s", path, e)
            return 1
        log.info("Writing to %s", os.path.abspath(path))

    server = None
    if args.tcp or args.socket:
        server = SocketServer()
        try:
            if args.tcp:
                await server.listen_tcp(*args.tcp)
            if args.socket:
                await server.listen_unix(args.socket)
        except OSError as e:
            log.error("Cannot open socket: %s", e)
            await server.close()
            return 1

    bridge = FlightBridge(output, server, args.name, args.raw)
    task = asyncio.create_task(bridge.run())

    # Ctrl-C / kill cancel the bridge; the file is valid JSON at every point anyway.
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, task.cancel)
        except (NotImplementedError, RuntimeError):
            pass  # Windows: Ctrl-C arrives as KeyboardInterrupt instead
    if args.duration:
        log.info("Recording for %g seconds", args.duration)
        loop.call_later(args.duration, task.cancel)

    exit_code = 0
    try:
        await task
    except asyncio.CancelledError:
        pass
    except BleakBluetoothNotAvailableError:
        exit_code = 1
    finally:
        if output:
            log.info("Stopping - recorded %d packets to %s", output.count, output.path)
            output.close()
        else:
            log.info("Stopping")
        if server:
            await server.close()
    return exit_code


def main(argv: list[str] | None = None) -> None:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.no_file and not (args.tcp or args.socket):
        parser.error("--no-file only makes sense together with --tcp or --socket")
    if args.no_file and args.output:
        parser.error("--no-file and -o can't be combined")
    setup_logging(args.quiet)
    try:
        sys.exit(asyncio.run(run(args)))
    except KeyboardInterrupt:
        sys.exit(0)

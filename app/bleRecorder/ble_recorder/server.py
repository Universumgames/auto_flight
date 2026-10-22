"""Newline-delimited JSON socket server over TCP and/or Unix domain sockets."""

from __future__ import annotations

import asyncio
import itertools
import os
import socket
import stat
from typing import Awaitable, Callable

from .protocol import log

# A client that falls this far behind is dropped instead of buffering without bound.
MAX_BUFFERED_BYTES = 4 * 1024 * 1024
MAX_LINE_BYTES = 1024 * 1024
# sockaddr_un.sun_path is 104 bytes on macOS (108 on Linux) including the terminating NUL.
MAX_UNIX_PATH_BYTES = 103


class Client:
    _ids = itertools.count(1)

    def __init__(self, label: str, writer: asyncio.StreamWriter):
        self.label = f"{label}#{next(self._ids)}"
        self.writer = writer


LineHandler = Callable[[str, Client], Awaitable[None]]


class SocketServer:
    def __init__(self):
        self.on_line: LineHandler | None = None
        self._servers: list[asyncio.base_events.Server] = []
        self._unix_paths: list[str] = []
        self._clients: set[Client] = set()
        self._tasks: set[asyncio.Task] = set()

    async def listen_tcp(self, host: str, port: int) -> None:
        label = f"tcp://{host}:{port}"
        server = await asyncio.start_server(
            lambda r, w: self._serve(label, r, w), host, port, limit=MAX_LINE_BYTES, reuse_address=True
        )
        self._servers.append(server)
        log.info("Listening on %s", label)

    async def listen_unix(self, path: str) -> None:
        """Listens on a Unix domain socket, replacing a stale socket file from a previous run."""
        if not hasattr(socket, "AF_UNIX") or not hasattr(asyncio, "start_unix_server"):
            raise OSError("Unix domain sockets are not supported on this platform, use --tcp")
        if len(os.fsencode(path)) > MAX_UNIX_PATH_BYTES:
            raise OSError(f"socket path too long (max {MAX_UNIX_PATH_BYTES} bytes): {path}")
        try:
            if not stat.S_ISSOCK(os.lstat(path).st_mode):
                raise OSError(f"{path} exists and is not a socket")
            os.unlink(path)
        except FileNotFoundError:
            pass
        label = f"unix://{path}"
        server = await asyncio.start_unix_server(
            lambda r, w: self._serve(label, r, w), path, limit=MAX_LINE_BYTES
        )
        self._servers.append(server)
        self._unix_paths.append(path)
        log.info("Listening on %s", label)

    async def _serve(self, label: str, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        client = Client(label, writer)
        self._clients.add(client)
        task = asyncio.current_task()
        self._tasks.add(task)
        log.info("Client connected (%s, %d total)", client.label, len(self._clients))
        try:
            while True:
                try:
                    raw = await reader.readuntil(b"\n")
                except asyncio.IncompleteReadError:
                    break  # EOF
                except asyncio.LimitOverrunError:
                    self.send('{"error":"line too long"}', client)
                    break
                line = raw.decode("utf-8", errors="replace").strip()
                if line and self.on_line:
                    await self.on_line(line, client)
        except (ConnectionError, OSError, asyncio.CancelledError):
            pass
        finally:
            self._tasks.discard(task)
            self._drop(client)

    def broadcast(self, line: str) -> None:
        for client in list(self._clients):
            self.send(line, client)

    def send(self, line: str, client: Client) -> None:
        if client not in self._clients:
            return
        transport = client.writer.transport
        if transport.is_closing():
            self._drop(client)
            return
        if transport.get_write_buffer_size() > MAX_BUFFERED_BYTES:
            log.warning("Client %s is not reading fast enough; dropping it", client.label)
            self._drop(client)
            return
        client.writer.write(line.encode("utf-8") + b"\n")

    def _drop(self, client: Client) -> None:
        if client not in self._clients:
            return
        self._clients.discard(client)
        client.writer.close()
        log.info("Client disconnected (%s, %d left)", client.label, len(self._clients))

    async def close(self) -> None:
        for client in list(self._clients):
            self._drop(client)
        # Handlers may still be waiting on the device; stop them before the loop shuts down.
        tasks = [t for t in self._tasks if t is not asyncio.current_task()]
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
        for server in self._servers:
            server.close()
        for path in self._unix_paths:
            try:
                os.unlink(path)
            except OSError:
                pass
        self._servers.clear()
        self._unix_paths.clear()

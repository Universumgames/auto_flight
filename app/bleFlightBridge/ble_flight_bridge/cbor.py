"""Minimal CBOR codec matching what the base station speaks.

The base station encodes every packet with nlohmann::json::to_cbor as a map keyed by field
name (shared_components/flight_com/PacketCodec.cpp), so a schema-less decode already yields
the full, named packet. A hand-written codec (instead of cbor2) lets float32 values come out
in their shortest form ("1010.08", not "1010.0800170898438") and keeps integers as integers
on the way back.
"""

from __future__ import annotations

import math
import struct
from typing import Any


class CBORError(ValueError):
    pass


def _shortest_float32(value: float) -> float:
    """Returns the shortest decimal that still round-trips to the same float32."""
    if not math.isfinite(value):
        return value
    packed = struct.pack(">f", value)
    for digits in range(6, 10):
        candidate = float(f"{value:.{digits}g}")
        if struct.pack(">f", candidate) == packed:
            return candidate
    return value


def _finite_or_none(value: float) -> float | None:
    return value if math.isfinite(value) else None


class _Decoder:
    def __init__(self, data: bytes):
        self.data = data
        self.pos = 0

    def read(self, n: int) -> bytes:
        if n < 0 or self.pos + n > len(self.data):
            raise CBORError("unexpected end of CBOR data")
        chunk = self.data[self.pos:self.pos + n]
        self.pos += n
        return chunk

    def argument(self, info: int) -> int | None:
        """Argument for additional info 0..27; None means indefinite length (31)."""
        if info < 24:
            return info
        if info in (24, 25, 26, 27):
            return int.from_bytes(self.read(1 << (info - 24)), "big")
        if info == 31:
            return None
        raise CBORError(f"invalid CBOR additional info {info}")

    def at_break(self) -> bool:
        return self.pos < len(self.data) and self.data[self.pos] == 0xFF

    def item(self) -> Any:
        initial = self.read(1)[0]
        major, info = initial >> 5, initial & 0x1F

        if major in (0, 1, 6):
            n = self.argument(info)
            if n is None:
                raise CBORError(f"invalid CBOR additional info {info}")
            if major == 0:
                return n
            if major == 1:
                return -1 - n
            return self.item()  # tag: keep only the tagged value
        if major in (2, 3):
            length = self.argument(info)
            if length is not None:
                raw = self.read(length)
            else:
                # Indefinite strings are a sequence of definite chunks.
                parts = []
                while not self.at_break():
                    header = self.read(1)[0]
                    chunk_len = self.argument(header & 0x1F)
                    if chunk_len is None:
                        raise CBORError("nested indefinite-length string")
                    parts.append(self.read(chunk_len))
                self.pos += 1
                raw = b"".join(parts)
            if major == 2:
                return raw.hex()
            try:
                return raw.decode("utf-8")
            except UnicodeDecodeError as e:
                raise CBORError("CBOR text string is not valid UTF-8") from e
        if major == 4:
            count = self.argument(info)
            if count is not None:
                return [self.item() for _ in range(count)]
            items = []
            while not self.at_break():
                items.append(self.item())
            self.pos += 1
            return items
        if major == 5:
            count = self.argument(info)
            result: dict[str, Any] = {}
            def add_pair():
                key = self.item()
                result[key if isinstance(key, str) else str(key)] = self.item()
            if count is not None:
                for _ in range(count):
                    add_pair()
            else:
                while not self.at_break():
                    add_pair()
                self.pos += 1
            return result

        # major 7: simple values and floats
        if info == 20:
            return False
        if info == 21:
            return True
        if info in (22, 23):
            return None
        if info == 24:
            return self.read(1)[0]
        # JSON has no NaN/Infinity, so those become None (null).
        if info == 25:
            return _finite_or_none(struct.unpack(">e", self.read(2))[0])
        if info == 26:
            return _finite_or_none(_shortest_float32(struct.unpack(">f", self.read(4))[0]))
        if info == 27:
            return _finite_or_none(struct.unpack(">d", self.read(8))[0])
        if info == 31:
            raise CBORError("unexpected CBOR break marker")
        if info < 20:
            return info
        raise CBORError(f"invalid CBOR additional info {info}")


def decode(data: bytes) -> Any:
    """Decodes one CBOR item into JSON-compatible Python values (byte strings become hex)."""
    decoder = _Decoder(bytes(data))
    value = decoder.item()
    if decoder.pos != len(decoder.data):
        raise CBORError(f"{len(decoder.data) - decoder.pos} trailing bytes after CBOR item")
    return value


def _head(major: int, argument: int) -> bytes:
    m = major << 5
    if argument < 24:
        return bytes([m | argument])
    for info, size in ((24, 1), (25, 2), (26, 4), (27, 8)):
        if argument < 1 << (8 * size):
            return bytes([m | info]) + argument.to_bytes(size, "big")
    raise CBORError(f"integer {argument} does not fit into 64 bits")


def encode(value: Any) -> bytes:
    """Encodes like nlohmann::json::to_cbor: definite lengths, shortest integer form.
    Floats stay float64; nlohmann's get<float>/get<int> converts on the C++ side."""
    if value is None:
        return b"\xf6"
    if value is True:
        return b"\xf5"
    if value is False:
        return b"\xf4"
    if isinstance(value, int):
        return _head(0, value) if value >= 0 else _head(1, -1 - value)
    if isinstance(value, float):
        return b"\xfb" + struct.pack(">d", value)
    if isinstance(value, str):
        raw = value.encode("utf-8")
        return _head(3, len(raw)) + raw
    if isinstance(value, (bytes, bytearray)):
        return _head(2, len(value)) + bytes(value)
    if isinstance(value, (list, tuple)):
        return _head(4, len(value)) + b"".join(encode(v) for v in value)
    if isinstance(value, dict):
        return _head(5, len(value)) + b"".join(encode(k) + encode(v) for k, v in value.items())
    raise CBORError(f"cannot encode {type(value).__name__} as CBOR")

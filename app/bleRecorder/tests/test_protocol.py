import asyncio
import json
import os
import tempfile
import unittest

from ble_recorder import cbor
from ble_recorder.cli import build_parser, parse_endpoint
from ble_recorder.jsonfile import JSONArrayFile
from ble_recorder.protocol import FragmentReassemblyBuffer, full_uuid, short_uuid, split_into_frames
from ble_recorder.server import SocketServer


class CBORTest(unittest.TestCase):
    def test_decodes_base_station_packet(self):
        # {"timestamp":1234,"type":16,"id":1,"pressure":float32(1013.25),"heading":-5,"a":[half(1.5)],"b":h'0102'}
        data = bytes.fromhex(
            "a76974696d657374616d701904d264747970651062696401687072657373757265fa447d5000"
            "6768656164696e672461618 1f93e0061624201 02".replace(" ", "")
        )
        self.assertEqual(
            cbor.decode(data),
            {"timestamp": 1234, "type": 16, "id": 1, "pressure": 1013.25, "heading": -5, "a": [1.5], "b": "0102"},
        )

    def test_float32_shortest_form(self):
        import struct
        self.assertEqual(cbor.decode(b"\xfa" + struct.pack(">f", 1010.08)), 1010.08)

    def test_nan_becomes_none(self):
        self.assertIsNone(cbor.decode(bytes.fromhex("f97e00")))

    def test_round_trip(self):
        packet = {
            "timestamp": 1792687521387, "type": 52, "id": 3499431421, "neg": -795535875,
            "min": -(2 ** 63), "big": 2 ** 64 - 1, "f": 6.498692962218165, "s": "é😀\"",
            "arr": [1, [], {}], "t": True, "n": None,
        }
        decoded = cbor.decode(cbor.encode(packet))
        self.assertEqual(decoded, packet)
        self.assertEqual(list(decoded), list(packet))  # key order preserved

    def test_shortest_integer_encoding(self):
        self.assertEqual(cbor.encode({"a": 1}).hex(), "a1616101")
        self.assertEqual(cbor.encode(500).hex(), "1901f4")
        self.assertEqual(cbor.encode(-1).hex(), "20")

    def test_rejects_garbage(self):
        for bad in (b"", b"\xa1", b"\x18", b"\x01\x02", b"\xff"):
            with self.assertRaises(cbor.CBORError):
                cbor.decode(bad)


class FragmentTest(unittest.TestCase):
    def test_split_and_reassemble(self):
        payload = bytes(range(256)) * 3
        frames = split_into_frames(payload, 100)
        self.assertEqual(len(frames), 8)
        buffer = FragmentReassemblyBuffer()
        results = [buffer.ingest(f) for f in frames]
        self.assertEqual(results[:-1], [None] * 7)
        self.assertEqual(results[-1], payload)

    def test_empty_payload_is_one_frame(self):
        self.assertEqual(split_into_frames(b"", 20), [b"\x00\x00\x00\x00"])

    def test_mismatched_fragment_is_dropped(self):
        buffer = FragmentReassemblyBuffer()
        self.assertIsNone(buffer.ingest(b"\x0a\x00\x00\x00abc"))
        self.assertIsNone(buffer.ingest(b"\x0b\x00\x03\x00def"))  # different total length
        self.assertIsNone(buffer.ingest(b"\x05\x00\x00\x00ab"))
        self.assertIsNone(buffer.ingest(b"\x05\x00\x04\x00overrun"))  # past the declared length
        self.assertEqual(buffer.ingest(b"\x02\x00\x00\x00ok"), b"ok")  # recovers afterwards

    def test_short_read_ack_is_ignored(self):
        self.assertIsNone(FragmentReassemblyBuffer().ingest(b""))


class UUIDTest(unittest.TestCase):
    def test_round_trip(self):
        self.assertEqual(full_uuid("AB11"), "0000ab11-0000-1000-8000-00805f9b34fb")
        self.assertEqual(short_uuid(full_uuid("ab11")), "AB11")
        custom = "12345678-1234-1234-1234-123456789abc"
        self.assertEqual(short_uuid(custom), custom.upper())


class JSONArrayFileTest(unittest.TestCase):
    def test_valid_after_every_append(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "out.json")
            f = JSONArrayFile(path)
            with open(path) as r:
                self.assertEqual(json.load(r), [])
            for i in range(3):
                f.append({"i": i, "nested": {"x": [i]}})
                with open(path) as r:
                    self.assertEqual(len(json.load(r)), i + 1)
            f.close()


class CLITest(unittest.TestCase):
    def test_endpoints(self):
        self.assertEqual(parse_endpoint("7878"), ("127.0.0.1", 7878))
        self.assertEqual(parse_endpoint("0.0.0.0:7878"), ("0.0.0.0", 7878))
        self.assertEqual(parse_endpoint("[::1]:7878"), ("::1", 7878))

    def test_rejects_bad_values(self):
        parser = build_parser()
        for argv in (["-t", "0"], ["-t", "abc"], ["--tcp", "x"], ["--tcp", "host:99999"]):
            with self.assertRaises(SystemExit), open(os.devnull, "w") as devnull:
                import contextlib
                with contextlib.redirect_stderr(devnull):
                    parser.parse_args(argv)


class SocketServerTest(unittest.IsolatedAsyncioTestCase):
    async def test_broadcast_and_reply(self):
        server = SocketServer()
        received = []

        async def on_line(line, client):
            received.append(line)
            server.send('{"ack":"AB11"}', client)

        server.on_line = on_line
        await server.listen_tcp("127.0.0.1", 0)
        port = server._servers[0].sockets[0].getsockname()[1]
        reader, writer = await asyncio.open_connection("127.0.0.1", port)
        writer.write(b'{"characteristic":"AB11"}\n')
        await writer.drain()
        self.assertEqual(await asyncio.wait_for(reader.readline(), 2), b'{"ack":"AB11"}\n')
        server.broadcast('{"packet":{}}')
        self.assertEqual(await asyncio.wait_for(reader.readline(), 2), b'{"packet":{}}\n')
        self.assertEqual(received, ['{"characteristic":"AB11"}'])
        writer.close()
        await server.close()


if __name__ == "__main__":
    unittest.main()

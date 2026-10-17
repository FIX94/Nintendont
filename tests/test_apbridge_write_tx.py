"""Local mock checks for the single-datagram WRTX client."""

import contextlib
import io
import runpy
import socket
import struct
import sys
import unittest
from unittest.mock import patch


CLIENT = runpy.run_path("scripts/apbridge_write_tx.py")
MAIN = CLIENT["main"]
TXID = 0x0123456789ABCDEF
ARGS = ["apbridge_write_tx.py", "192.168.0.153", "0x80000000", "475A4C453939"]


class FakeSocket:
    def __init__(self, reply):
        self.reply = reply
        self.sent = []

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        return False

    def settimeout(self, timeout):
        assert timeout == 5

    def connect(self, endpoint):
        assert endpoint == ("192.168.0.153", 55000)

    def send(self, packet):
        self.sent.append(packet)

    def recv(self, limit):
        assert limit >= 20
        return self.reply


class WriteTxClientTests(unittest.TestCase):
    def run_client(self, reply, extra_args=None):
        fake = FakeSocket(reply)
        output = io.StringIO()
        argv = ARGS + (extra_args if extra_args is not None else ["--txid", hex(TXID)])
        with patch.object(socket, "socket", return_value=fake), \
                patch.object(sys, "argv", argv), contextlib.redirect_stdout(output):
            result = MAIN()
        return result, output.getvalue(), fake.sent

    def test_success_and_one_packet(self):
        reply = struct.pack(">4sIHBBQ", b"WROK", 0x80000000, 6, 0, 0, TXID)
        result, output, sent = self.run_client(reply)
        self.assertEqual(result, 0)
        self.assertEqual(sent, [struct.pack(">4sIHHQ", b"WRTX",
                                            0x80000000, 6, 0, TXID) + b"GZLE99"])
        self.assertIn(f"0x{TXID:016X}", output)
        self.assertIn("Write acknowledged", output)

    def test_generated_id_and_conflict(self):
        reply = struct.pack(">4sIHBBQ", b"ERR!", 0x80000000, 6, 6, 0, TXID)
        with patch.object(CLIENT["secrets"], "randbits", return_value=TXID):
            result, output, sent = self.run_client(reply, [])
        self.assertEqual(result, 1)
        self.assertEqual(len(sent), 1)
        self.assertIn("transaction ID conflict", output)
        self.assertIn(f"0x{TXID:016X}", output)

    def test_bad_replies(self):
        good = struct.pack(">4sIHBBQ", b"WROK", 0x80000000, 6, 0, 0, TXID)
        cases = [
            good[:-1],
            struct.pack(">4sIHBBQ", b"NOPE", 0x80000000, 6, 0, 0, TXID),
            struct.pack(">4sIHBBQ", b"WROK", 0x80000001, 6, 0, 0, TXID),
            struct.pack(">4sIHBBQ", b"WROK", 0x80000000, 5, 0, 0, TXID),
            struct.pack(">4sIHBBQ", b"WROK", 0x80000000, 6, 0, 1, TXID),
            struct.pack(">4sIHBBQ", b"WROK", 0x80000000, 6, 0, 0, TXID + 1),
            struct.pack(">4sIHBBQ", b"ERR!", 0x80000000, 6, 7, 0, TXID),
        ]
        for reply in cases:
            with self.subTest(reply=reply):
                result, _output, sent = self.run_client(reply)
                self.assertEqual(result, 1)
                self.assertEqual(len(sent), 1)

    def test_input_rejections_before_network(self):
        for address, data, extra in [
            ("0x80000000", "", []),
            ("0x80000000", "A", []),
            ("0x80000000", "ZZ", []),
            ("0x80000000", "00" * 257, []),
            ("0x7FFFFFFF", "00", []),
            ("0x817FFFFF", "0000", []),
            ("0x81800000", "00", []),
            ("0x80000000", "00", ["--txid", "0"]),
            ("0x80000000", "00", ["--txid", str(1 << 64)]),
        ]:
            with self.subTest(address=address, data=data[:8], extra=extra):
                argv = ["apbridge_write_tx.py", "192.168.0.153", address, data] + extra
                with patch.object(sys, "argv", argv), \
                        patch.object(socket, "socket") as mock_socket, \
                        contextlib.redirect_stderr(io.StringIO()):
                    with self.assertRaises(SystemExit) as raised:
                        MAIN()
                self.assertEqual(raised.exception.code, 2)
                mock_socket.assert_not_called()


if __name__ == "__main__":
    unittest.main()

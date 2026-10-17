#!/usr/bin/env python3
"""Send one idempotent AP Bridge WRTX request to GameCube MEM1."""

import argparse
import ipaddress
import secrets
import socket
import string
import struct


PORT = 55000
MEM1_START = 0x80000000
MEM1_END = 0x81800000
ERRORS = {
    1: "invalid length",
    2: "invalid MEM1 address or range",
    3: "reserved field is nonzero",
    4: "WRITE payload length mismatch",
    5: "invalid transaction ID",
    6: "transaction ID conflict",
}


def parse_number(value):
    return int(value, 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wii_ip", type=ipaddress.IPv4Address)
    parser.add_argument("address", type=parse_number)
    parser.add_argument("hex_data")
    parser.add_argument("--txid", type=parse_number,
                        help="nonzero 64-bit ID; reuse only with the identical address and payload")
    args = parser.parse_args()

    if not args.hex_data:
        parser.error("HEX_DATA must contain at least one byte")
    if len(args.hex_data) % 2:
        parser.error("HEX_DATA must have an even number of hex digits")
    if any(character not in string.hexdigits for character in args.hex_data):
        parser.error("HEX_DATA must contain only hexadecimal digits")
    payload = bytes.fromhex(args.hex_data)
    if len(payload) > 256:
        parser.error("HEX_DATA must contain no more than 256 bytes")
    if not MEM1_START <= args.address < MEM1_END \
            or len(payload) > MEM1_END - args.address:
        parser.error("ADDRESS and payload must fit entirely within GameCube MEM1")
    if args.txid is not None and not 1 <= args.txid <= 0xFFFFFFFFFFFFFFFF:
        parser.error("TXID must be a nonzero unsigned 64-bit integer")

    txid = args.txid
    if txid is None:
        txid = 0
        while txid == 0:
            txid = secrets.randbits(64)

    endpoint = (str(args.wii_ip), PORT)
    request = struct.pack(">4sIHHQ", b"WRTX", args.address,
                          len(payload), 0, txid) + payload
    print(f"Transaction ID: 0x{txid:016X}")
    print(f"Sending WRTX 0x{args.address:08X} length {len(payload)} "
          f"to {endpoint[0]}:{endpoint[1]}")
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(5)
            sock.connect(endpoint)  # Accept replies only from this Wii endpoint.
            sock.send(request)  # Exactly one request; manual retries reuse --txid.
            reply = sock.recv(1024)
    except socket.timeout:
        print("Timed out waiting for the Wii (5 seconds). Retry manually with the same --txid.")
        return 1
    except OSError as error:
        print(f"Network error: {error}")
        return 1

    if len(reply) != 20:
        print(f"Invalid response length: {len(reply)} bytes (expected 20).")
        return 1
    magic, address, length, code, reserved, reply_txid = struct.unpack(">4sIHBBQ", reply)
    if magic not in (b"WROK", b"ERR!"):
        print(f"Invalid response magic: {magic!r}")
        return 1
    if address != args.address or length != len(payload) or reply_txid != txid:
        print("Invalid response echo (address, length, or transaction ID).")
        return 1
    if reserved != 0:
        print("Invalid response reserved field.")
        return 1
    if magic == b"ERR!":
        if code not in ERRORS:
            print(f"Invalid error code: {code}.")
            return 1
        print(f"Wii rejected WRTX: {ERRORS[code]} (code {code}).")
        return 1
    if code != 0:
        print(f"Invalid WROK status byte: {code}.")
        return 1

    print(f"Write acknowledged: 0x{address:08X}, {length} bytes "
          f"(transaction 0x{txid:016X})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

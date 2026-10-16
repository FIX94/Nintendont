#!/usr/bin/env python3
"""Write 1..256 bytes to GameCube MEM1 through AP Bridge v0.4."""

import argparse
import ipaddress
import socket
import string
import struct


ERRORS = {
    1: "invalid length",
    2: "invalid MEM1 address or range",
    3: "reserved field is nonzero",
    4: "WRITE payload length mismatch",
}


def parse_number(value):
    return int(value, 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wii_ip", type=ipaddress.IPv4Address)
    parser.add_argument("address", type=parse_number)
    parser.add_argument("hex_data")
    args = parser.parse_args()

    if not 0 <= args.address <= 0xFFFFFFFF:
        parser.error("ADDRESS must fit in an unsigned 32-bit integer")
    if not args.hex_data:
        parser.error("HEX_DATA must contain at least one byte")
    if len(args.hex_data) % 2:
        parser.error("HEX_DATA must have an even number of hex digits")
    if any(character not in string.hexdigits for character in args.hex_data):
        parser.error("HEX_DATA must contain only hexadecimal digits")
    payload = bytes.fromhex(args.hex_data)
    if len(payload) > 256:
        parser.error("HEX_DATA must contain no more than 256 bytes")

    endpoint = (str(args.wii_ip), 55000)
    request = struct.pack(">4sIHH", b"WRIT", args.address, len(payload), 0) + payload
    print(f"Sending WRITE 0x{args.address:08X} length {len(payload)} "
          f"to {endpoint[0]}:{endpoint[1]}")
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(5)
            sock.connect(endpoint)  # UDP: accept replies only from this endpoint.
            sock.send(request)
            reply = sock.recv(1024)
    except socket.timeout:
        print("Timed out waiting for the Wii (5 seconds).")
        return 1
    except OSError as error:
        print(f"Network error: {error}")
        return 1

    if len(reply) != 12:
        print(f"Invalid response length: {len(reply)} bytes (expected 12).")
        return 1
    magic, address, length, code, reserved = struct.unpack(">4sIHBB", reply)
    if address != args.address or length != len(payload):
        print(f"Invalid echo: address 0x{address:08X}, length {length}.")
        return 1
    if reserved != 0:
        print("Invalid response reserved field.")
        return 1
    if magic == b"ERR!":
        print(f"Wii rejected WRITE: {ERRORS.get(code, f'unknown error {code}')} "
              f"(code {code}).")
        return 1
    if magic != b"WROK" or code != 0:
        print(f"Invalid WRITE acknowledgment: magic {magic!r}, reserved/code {code}.")
        return 1

    print(f"Write acknowledged: 0x{address:08X}, {length} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

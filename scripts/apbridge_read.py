#!/usr/bin/env python3
"""Read up to 256 bytes of GameCube MEM1 through AP Bridge v0.3."""

import argparse
import ipaddress
import socket
import struct


ERRORS = {
    1: "invalid length",
    2: "invalid MEM1 address or range",
    3: "reserved field is nonzero",
}


def parse_number(value):
    return int(value, 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wii_ip", type=ipaddress.IPv4Address)
    parser.add_argument("address", nargs="?", type=parse_number, default=0x80000000)
    parser.add_argument("length", nargs="?", type=parse_number, default=6)
    args = parser.parse_args()

    if not 0 <= args.address <= 0xFFFFFFFF:
        parser.error("ADDRESS must fit in an unsigned 32-bit integer")
    if not 0 <= args.length <= 0xFFFF:
        parser.error("LENGTH must fit in an unsigned 16-bit integer")

    endpoint = (str(args.wii_ip), 55000)
    request = struct.pack(">4sIHH", b"READ", args.address, args.length, 0)
    print(f"Sending READ 0x{args.address:08X} length {args.length} "
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

    if len(reply) < 12:
        print(f"Invalid short response ({len(reply)} bytes).")
        return 1
    magic, address, length = struct.unpack(">4sIH", reply[:10])
    if address != args.address or length != args.length:
        print(f"Invalid echo: address 0x{address:08X}, length {length}.")
        return 1

    if magic == b"ERR!":
        if len(reply) != 12 or reply[11] != 0:
            print("Invalid ERR! response length or reserved field.")
            return 1
        code = reply[10]
        print(f"Wii rejected READ: {ERRORS.get(code, f'unknown error {code}')} "
              f"(code {code}).")
        return 1
    if magic != b"DATA":
        print(f"Unexpected response magic: {magic!r}")
        return 1
    if reply[10:12] != b"\0\0" or len(reply) != 12 + length:
        print("Invalid DATA response length or reserved field.")
        return 1

    data = reply[12:]
    print(f"Received DATA for 0x{address:08X}, {len(data)} bytes")
    print("Hex:   " + " ".join(f"{byte:02X}" for byte in data))
    print("ASCII: " + "".join(chr(byte) if 32 <= byte <= 126 else "." for byte in data))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

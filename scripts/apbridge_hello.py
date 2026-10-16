#!/usr/bin/env python3
"""Send the AP Bridge v0.2 handshake to a real Wii on the local network."""

import argparse
import ipaddress
import socket


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wii_ip", type=ipaddress.IPv4Address)
    args = parser.parse_args()
    address = (str(args.wii_ip), 55000)

    print(f"Sending HELLO to {address[0]}:{address[1]}")
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(5)
            sock.connect(address)  # UDP: accept replies only from this endpoint.
            sock.send(b"HELLO")
            reply = sock.recv(64)
    except socket.timeout:
        print("Timed out waiting for the Wii (5 seconds).")
        return 1
    except OSError as error:
        print(f"Network error: {error}")
        return 1

    print(f"Received: {reply.decode('ascii', errors='backslashreplace')}")
    if reply != b"NIN-AP":
        print(f"Unexpected reply bytes: {reply!r}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

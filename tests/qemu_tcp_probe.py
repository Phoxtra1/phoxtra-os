#!/usr/bin/env python3
"""Host-side peer for the guest's one-shot QEMU TCP integration probe."""
import argparse
import socket
import sys

REQUEST = b"PHOXTRA_TCP_PROBE"
EXPECTED = b"PHOXTRA_TCP_E2E_OK"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", type=int, help="QEMU host-forwarded TCP port")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("port must be in the range 1..65535")
    try:
        with socket.create_connection(("127.0.0.1", args.port), timeout=10) as peer:
            peer.settimeout(10)
            peer.sendall(REQUEST)
            received = bytearray()
            while len(received) < len(EXPECTED):
                chunk = peer.recv(len(EXPECTED) - len(received))
                if not chunk:
                    break
                received.extend(chunk)
            if bytes(received) != EXPECTED:
                print(f"FAIL: expected {EXPECTED!r}, received {bytes(received)!r}", file=sys.stderr)
                return 1
    except (OSError, TimeoutError) as exc:
        print(f"FAIL: QEMU guest TCP probe did not complete: {exc}", file=sys.stderr)
        return 1
    print("PASS: guest E1000/ARP/IPv4/TCP socket exchange completed end to end.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

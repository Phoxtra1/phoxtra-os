#!/usr/bin/env python3
"""Single-connection TLS origin used to verify guest HTTPS over E1000/TCP."""
import argparse
import socket
import ssl
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cert", required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument("--port", type=int, default=44243)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("port must be in the range 1..65535")
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.maximum_version = ssl.TLSVersion.TLSv1_3
    context.load_cert_chain(args.cert, args.key)
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", args.port))
    server.listen(1)
    server.settimeout(120)
    print(f"LISTENING {args.port}", flush=True)
    try:
        connection, _ = server.accept()
        connection.settimeout(20)
        tls = context.wrap_socket(connection, server_side=True)
        try:
            request = bytearray()
            while b"\r\n\r\n" not in request and len(request) < 4096:
                part = tls.recv(512)
                if not part:
                    raise RuntimeError("TLS HTTP request ended early")
                request.extend(part)
            if not request.startswith(b"GET / HTTP/1.1\r\n"):
                raise RuntimeError("guest sent an unexpected HTTP request line")
            print("HTTPS_SERVER_REQUEST_OK GET /", flush=True)
            body = b"PHOXTRA_GUEST_HTTPS_OK"
            tls.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
                        + b"Content-Length: " + str(len(body)).encode("ascii")
                        + b"\r\nConnection: close\r\n\r\n" + body)
            # Send close_notify, then accept the client's reciprocal close_notify.
            raw = tls.unwrap()
            raw.close()
        except (ssl.SSLError, OSError, RuntimeError, TimeoutError) as exc:
            print(f"HTTPS guest test server error: {exc}", file=sys.stderr, flush=True)
            tls.close()
            return 1
    finally:
        server.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

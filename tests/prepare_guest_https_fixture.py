#!/usr/bin/env python3
"""Create short-lived local HTTPS credentials for the guest TCP/TLS regression."""
import argparse
import pathlib
import subprocess


def run(*args: str) -> None:
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output_dir", type=pathlib.Path)
    args = parser.parse_args()
    output = args.output_dir
    output.mkdir(parents=True, exist_ok=True)
    ca_key, ca_pem = output / "ca.key", output / "ca.pem"
    server_key, server_csr = output / "server.key", output / "server.csr"
    server_pem, extensions = output / "server.pem", output / "server.ext"
    header = output.parent / "generated" / "phoxtra_guest_test_ca.h"
    run("openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", str(ca_key))
    run("openssl", "req", "-x509", "-new", "-key", str(ca_key), "-sha256", "-days", "3",
        "-subj", "/CN=Phoxtra Guest HTTPS Test Root", "-addext", "basicConstraints=critical,CA:TRUE",
        "-addext", "keyUsage=critical,keyCertSign,cRLSign", "-out", str(ca_pem))
    run("openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", str(server_key))
    run("openssl", "req", "-new", "-key", str(server_key), "-subj", "/CN=10.0.2.2", "-out", str(server_csr))
    extensions.write_text("basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\n"
                          "extendedKeyUsage=serverAuth\nsubjectAltName=IP:10.0.2.2\n", encoding="ascii")
    run("openssl", "x509", "-req", "-in", str(server_csr, ), "-CA", str(ca_pem), "-CAkey", str(ca_key),
        "-CAcreateserial", "-out", str(server_pem), "-days", "2", "-sha256", "-extfile", str(extensions))
    pem_lines = ca_pem.read_text(encoding="ascii").splitlines()
    lines = ["#ifndef PHOXTRA_GUEST_TEST_CA_H", "#define PHOXTRA_GUEST_TEST_CA_H",
             "static const unsigned char phoxtra_guest_test_ca_pem[] ="]
    lines.extend('    "' + line + r'\n"' for line in pem_lines)
    lines.extend((";", "#endif", ""))
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text("\n".join(lines), encoding="ascii")
    print("Prepared isolated guest HTTPS test certificate and CA header.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

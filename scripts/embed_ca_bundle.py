#!/usr/bin/env python3
"""Embed a pinned certifi PEM bundle as a NUL-terminated C string."""
import json
import pathlib
import sys
def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: embed_ca_bundle.py INPUT_PEM OUTPUT_HEADER")
    source=pathlib.Path(sys.argv[1])
    output=pathlib.Path(sys.argv[2])
    pem=source.read_bytes().decode("ascii")
    if "-----BEGIN CERTIFICATE-----" not in pem:
        raise SystemExit("CA source does not contain a PEM certificate")
    if len(pem)>2_000_000:
        raise SystemExit("CA bundle exceeds configured size limit")
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(
        "#ifndef PHOXTRA_DEFAULT_CA_GENERATED_H\n#define PHOXTRA_DEFAULT_CA_GENERATED_H\n"
        "#include <stddef.h>\nstatic const unsigned char phoxtra_default_ca_pem[] =\n"
        +json.dumps(pem,ensure_ascii=True)+";\n"
        "#define PHOXTRA_DEFAULT_CA_PEM_LENGTH (sizeof(phoxtra_default_ca_pem))\n#endif\n",
        encoding="ascii")
if __name__=="__main__":main()

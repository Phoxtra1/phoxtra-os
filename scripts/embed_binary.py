#!/usr/bin/env python3
"""Embed a binary as a static byte array for a freestanding kernel test image."""
import pathlib
import re
import sys


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit("usage: embed_binary.py INPUT OUTPUT SYMBOL")
    source, output, symbol = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3]
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", symbol):
        raise SystemExit("invalid C symbol name")
    data = source.read_bytes()
    if not data or len(data) > 16 * 1024 * 1024:
        raise SystemExit("embedded binary is empty or exceeds 16 MiB")
    output.parent.mkdir(parents=True, exist_ok=True)
    lines = ["#include <stddef.h>", "#include <stdint.h>", f"static const uint8_t {symbol}_data[] = {{"]
    for start in range(0, len(data), 12):
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in data[start:start + 12]) + ",")
    lines += ["};", f"const uint8_t* {symbol}_bytes(void) {{ return {symbol}_data; }}",
              f"size_t {symbol}_size(void) {{ return sizeof({symbol}_data); }}", ""]
    output.write_text("\n".join(lines), encoding="ascii")
    print(f"Embedded {len(data)} bytes from {source}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

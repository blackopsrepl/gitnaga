#!/usr/bin/env python3
"""Writes one side of the zoo's `media` fixture into a directory.

    fixture_media.py <dir> before|after

The two sides differ in every way the diff view must handle: an image, a
binary blob, C++ and Python edits (syntax and word highlights), a
whitespace-only change, and a 20 000-line file with changes spread through it.
"""
import struct
import sys
import zlib
from pathlib import Path


def png(width, height, pixel):
    rows = b"".join(b"\0" + b"".join(bytes(pixel(x, y)) for x in range(width)) for y in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


CPP = {
    "before": """#include <vector>

// Sum of the weights the solver keeps.
int total(const std::vector<int> &weights)
{
    int sum = 0;
    for (int weight : weights)
        sum += weight;
    return sum;
}
""",
    "after": """#include <numeric>
#include <vector>

// Sum of the weights the solver keeps, widened so a long run cannot overflow.
long long total(const std::vector<int> &weights)
{
    return std::accumulate(weights.begin(), weights.end(), 0LL);
}
""",
}

PYTHON = {
    "before": """def score(entries, limit=10):
    \"\"\"Best entries first.\"\"\"
    ranked = sorted(entries, key=lambda e: e.value)
    return ranked[:limit]
""",
    "after": """def score(entries, limit=25):
    \"\"\"Best entries first, ties broken by name.\"\"\"
    ranked = sorted(entries, key=lambda e: (-e.value, e.name))
    return ranked[:limit]
""",
}


def write(root: Path, side: str):
    after = side == "after"
    blue, green = (40, 90, 200), (40, 170, 90)
    (root / "logo.png").write_bytes(png(96, 64, lambda x, y: green if after and (x // 12 + y // 12) % 2 else blue))
    (root / "model.bin").write_bytes(bytes(range(256)) * (8 if after else 4))
    (root / "total.cpp").write_text(CPP[side])
    (root / "score.py").write_text(PYTHON[side])
    indent = "\t" if after else "    "
    (root / "spacing.txt").write_text("".join(f"{indent}line {i}\n" for i in range(8)))
    (root / "large.txt").write_text(
        "".join(f"row {i} {'changed' if after and i % 400 == 0 else 'value'}\n" for i in range(20000)))


if __name__ == "__main__":
    write(Path(sys.argv[1]), sys.argv[2])

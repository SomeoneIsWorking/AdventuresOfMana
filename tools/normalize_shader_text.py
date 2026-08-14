#!/usr/bin/env python3
"""Canonicalize generated textual shaders without changing their code."""

from __future__ import annotations

import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} FILE", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    lines = [line.rstrip() for line in path.read_text().splitlines()]
    while lines and not lines[-1]:
        lines.pop()
    if not lines:
        print(f"FATAL: generated text shader {path} is empty", file=sys.stderr)
        return 2
    path.write_text("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

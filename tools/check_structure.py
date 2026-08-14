#!/usr/bin/env python3
"""Reject new god files and prevent known legacy monoliths from growing."""

from __future__ import annotations

import argparse
from pathlib import Path


DEFAULT_LIMIT = 1000
LEGACY_LIMITS = {
    "src/host/main.cpp": 7617,
    "src/mcf/assets.cpp": 1323,
}
SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hh", ".hpp"}


def violations(measured: dict[str, int]) -> list[str]:
    failures = []
    for name, lines in sorted(measured.items()):
        limit = LEGACY_LIMITS.get(name, DEFAULT_LIMIT)
        if lines > limit:
            kind = "legacy monolith grew" if name in LEGACY_LIMITS else "oversized module"
            failures.append(
                f"{name}: {lines} lines exceeds {limit} ({kind})"
            )
    return failures


def scan(root: Path) -> dict[str, int]:
    source_root = root / "src"
    files = sorted(path for path in source_root.rglob("*") if path.suffix in SUFFIXES)
    if not files:
        raise RuntimeError(f"scanned 0 source files under {source_root}")
    return {
        str(path.relative_to(root)): sum(1 for _ in path.open("rb"))
        for path in files
    }


def selftest() -> int:
    accepted = violations({"src/new/cohesive.cpp": DEFAULT_LIMIT})
    rejected = violations({"src/new/god.cpp": DEFAULT_LIMIT + 1})
    legacy = violations({"src/host/main.cpp": LEGACY_LIMITS["src/host/main.cpp"] + 1})
    if accepted or len(rejected) != 1 or len(legacy) != 1:
        print(
            "SELFTEST FAIL: structure gate did not distinguish accepted, new-god, "
            "and legacy-growth cases"
        )
        return 1
    print("structure selftest: accepted 1 bounded module; rejected 1 oversized module")
    print("structure selftest: rejected growth of 1 legacy monolith")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        return selftest()

    root = Path(__file__).resolve().parent.parent
    try:
        measured = scan(root)
    except RuntimeError as exc:
        print(f"STRUCTURE FAIL: {exc}")
        return 2
    failures = violations(measured)
    print(f"structure: scanned {len(measured)} source files; {len(failures)} violation(s)")
    for failure in failures:
        print(f"STRUCTURE FAIL: {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())

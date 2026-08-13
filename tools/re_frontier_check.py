#!/usr/bin/env python3
"""Validate the structured entries in docs/re-frontier.md.

The project keeps detailed legacy tables below the structured execution spine.
This checker deliberately reads only `### id — title` entries and their field
lines. Zero parsed entries is a hard failure: a schema mismatch must never look
like an exhausted or clean frontier.
"""
import re
import sys
from pathlib import Path

VALID = {
    "re-verified", "re-partial", "in-progress", "hack", "authored",
    "todo", "skip-by-design",
}


def load(path: Path):
    entries = {}
    current = None
    for raw in path.read_text(encoding="utf-8").splitlines():
        match = re.match(r"^### +(\S+) +(?:—|-) +(.+)$", raw)
        if match:
            current = {
                "id": match.group(1), "title": match.group(2), "status": "",
                "deps": [], "evidence": "", "where": "", "gap": "",
            }
            if current["id"] in entries:
                raise ValueError(f"duplicate entry {current['id']}")
            entries[current["id"]] = current
            continue
        match = re.match(r"^- +(status|deps|evidence|where|gap): ?(.*)$", raw)
        if match and current:
            key, value = match.groups()
            current[key] = ([v.strip() for v in value.split(",") if v.strip()]
                            if key == "deps" else value.strip())
    return entries


def check(path: Path) -> int:
    try:
        entries = load(path)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"RE-FRONTIER FAIL: could not validate {path}: {error}",
              file=sys.stderr)
        return 1
    if not entries:
        print(f"RE-FRONTIER FAIL: parsed 0 structured entries from {path}; "
              "verified nothing", file=sys.stderr)
        return 1

    failures = []
    for entry in entries.values():
        if entry["status"] not in VALID:
            failures.append(f"{entry['id']}: invalid/missing status "
                            f"{entry['status']!r}")
        if entry["status"] == "re-verified" and not entry["evidence"]:
            failures.append(f"{entry['id']}: re-verified without evidence")
        for dep in entry["deps"]:
            if dep not in entries:
                failures.append(f"{entry['id']}: unknown dependency {dep}")

    visiting, visited = set(), set()

    def visit(entry_id, chain):
        if entry_id in visiting:
            failures.append("dependency cycle: " + " -> ".join(chain + [entry_id]))
            return
        if entry_id in visited:
            return
        visiting.add(entry_id)
        for dep in entries[entry_id]["deps"]:
            if dep in entries:
                visit(dep, chain + [entry_id])
        visiting.remove(entry_id)
        visited.add(entry_id)

    for entry_id in entries:
        visit(entry_id, [])

    if failures:
        for failure in failures:
            print(f"RE-FRONTIER FAIL: {failure}", file=sys.stderr)
        print(f"RE-FRONTIER FAIL: {len(failures)} problem(s) across "
              f"{len(entries)} entries", file=sys.stderr)
        return 1

    hacks = sum(e["status"] == "hack" for e in entries.values())
    print(f"RE-FRONTIER OK: {len(entries)} entries parsed, 0 unknown deps, "
          f"0 cycles, {hacks} explicit hack debt(s)")
    return 0


if __name__ == "__main__":
    roadmap = Path(sys.argv[1] if len(sys.argv) > 1 else "docs/re-frontier.md")
    raise SystemExit(check(roadmap))

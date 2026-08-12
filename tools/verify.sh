#!/usr/bin/env bash
# Run every asset parser over the full extracted corpus.
# Fails loudly on an empty corpus rather than reporting a vacuous pass.
set -uo pipefail
cd "$(dirname "$0")/.."

n=$(find scratch/dump -type f 2>/dev/null | wc -l)
if [ "$n" -lt 9886 ]; then
  echo "FATAL: scratch/dump holds $n files, expected 9886."
  echo "       Run: python3 tools/asset/mpk.py <sk1.mpk> -o scratch/dump"
  exit 1
fi
echo "corpus: $n files"

fail=0
for t in stex smdl smot scol roomdata strings enemydat; do
  echo "=== $t ==="
  python3 "tools/asset/$t.py" || fail=1
done
if [ -x build/mana ] && [ -d scratch/raw/assets ]; then
  echo "=== combat self-test ==="
  ./build/mana --combat-selftest >/dev/null || fail=1
  echo "  (18 cases: 9 hit/miss geometry, 9 faction filter)"

  echo "=== player self-test ==="
  ./build/mana --player-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || fail=1
  ./build/mana --player-selftest >/dev/null 2>&1 || fail=1

  echo "=== inventory self-test ==="
  ./build/mana --inventory-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || fail=1
  ./build/mana --ai-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || fail=1
  ./build/mana --ai-selftest >/dev/null 2>&1 || fail=1
  ./build/mana --mode-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || fail=1
  ./build/mana --mode-selftest >/dev/null 2>&1 || fail=1
  ./build/mana --png-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || fail=1
  ./build/mana --png-selftest >/dev/null 2>&1 || fail=1
  ./build/mana --inventory-selftest >/dev/null 2>&1 || fail=1

  echo "=== text self-test ==="
  for l in en ja; do
    ./build/mana --text-selftest --lang "$l" 2>&1 | grep -E "SELFTEST:|sweep" || fail=1
    ./build/mana --text-selftest --lang "$l" >/dev/null 2>&1 || fail=1
  done

  echo "=== audio self-test ==="
  SDL_AUDIODRIVER=dummy ./build/mana --audio-selftest || fail=1

  # Loads every room in the game headlessly. Non-zero on a mesh/script failure
  # or an unresolved object id.
  echo "=== room census ==="
  ./build/mana --room-census 2>&1 | grep "^\[census\]" || fail=1
  ./build/mana --room-census >/dev/null 2>&1 || fail=1
else
  echo "=== audio self-test SKIPPED (build/mana or scratch/raw/assets missing) ==="
fi

echo "=== cmd API ==="
python3 tools/asset/extract_cmd_api.py >/dev/null || fail=1

# Regenerates docs/object-table.md and src/engine/object_table.inc, and checks
# every .odt id still resolves. Needs the game binary, which is not in the repo.
if [ -f scratch/raw/libmcfandroid.so ]; then
  echo "=== map-object table ==="
  python3 tools/asset/object_table.py || fail=1
  echo "=== weapon table ==="
  python3 tools/asset/weapon_table.py || fail=1
  echo "=== item table ==="
  python3 tools/asset/item_table.py || fail=1
  echo "=== world map ==="
  python3 tools/asset/worldmap.py --check || fail=1
else
  echo "=== map-object table SKIPPED (scratch/raw/libmcfandroid.so missing) ==="
fi

[ "$fail" -eq 0 ] && echo "ALL PARSERS PASSED" || echo "FAILURES ABOVE"
exit "$fail"

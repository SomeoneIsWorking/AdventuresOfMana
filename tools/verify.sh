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
for t in stex smdl smot scol; do
  echo "=== $t ==="
  python3 "tools/asset/$t.py" || fail=1
done
if [ -x build/mana ] && [ -d scratch/raw/assets ]; then
  echo "=== audio self-test ==="
  SDL_AUDIODRIVER=dummy ./build/mana --audio-selftest || fail=1
else
  echo "=== audio self-test SKIPPED (build/mana or scratch/raw/assets missing) ==="
fi

echo "=== cmd API ==="
python3 tools/asset/extract_cmd_api.py >/dev/null || fail=1

[ "$fail" -eq 0 ] && echo "ALL PARSERS PASSED" || echo "FAILURES ABOVE"
exit "$fail"

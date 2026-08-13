#!/usr/bin/env bash
# Run every asset parser over the full extracted corpus.
# Fails loudly on an empty corpus rather than reporting a vacuous pass.
set -uo pipefail
cd "$(dirname "$0")/.."

check_runtime() {
  if [ ! -x "$1" ]; then
    echo "FATAL: game binary $1 is missing or not executable; 0 gameplay checks ran."
    return 1
  fi
  if [ ! -d "$2" ]; then
    echo "FATAL: runtime asset directory $2 is missing; 0 gameplay checks ran."
    return 1
  fi
}

check_runtime build/mana scratch/raw/assets || exit 1
mkdir -p scratch/logs

archive=scratch/raw/assets/sk1/sk1.mpk
if [ ! -f "$archive" ]; then
  echo "FATAL: $archive is missing; corpus identity cannot be verified."
  echo "       A file count alone cannot prove scratch/dump came from this archive."
  exit 1
fi
python3 tools/asset/mpk.py "$archive" --check-dir scratch/dump || exit 1

fail=0

echo "=== runtime preflight negatives ==="
preflight_log=scratch/logs/runtime-preflight-negative.log
if check_runtime scratch/logs/DOES_NOT_EXIST scratch/raw/assets \
    >"$preflight_log" 2>&1; then
  fail=1
fi
grep -F "0 gameplay checks ran" "$preflight_log" >/dev/null || fail=1
if check_runtime build/mana scratch/logs/DOES_NOT_EXIST \
    >"$preflight_log" 2>&1; then
  fail=1
fi
grep -F "0 gameplay checks ran" "$preflight_log" >/dev/null || fail=1
echo "  missing-binary and missing-assets negatives passed"

# Prove exact corpus identity can produce the other answer. Work on one known
# member with an EXIT trap so interruption restores it before any parser runs.
echo "=== MPK corpus identity negatives ==="
corpus_member=scratch/dump/sk1/M0001_00_00.lua
corpus_hold=scratch/logs/mpk-corpus-held.lua
corpus_log=scratch/logs/mpk-corpus-negative.log
(
  restore_corpus_member() {
    if [ -f "$corpus_hold" ]; then
      rm -f "$corpus_member"
      mv "$corpus_hold" "$corpus_member"
    fi
  }
  trap restore_corpus_member EXIT INT TERM
  mv "$corpus_member" "$corpus_hold"
  if python3 tools/asset/mpk.py "$archive" --check-dir scratch/dump \
      >"$corpus_log" 2>&1; then
    exit 1
  fi
  grep -F "9886 expected, 9885 present; 1 missing" "$corpus_log" >/dev/null || exit 1
  : > "$corpus_member"
  if python3 tools/asset/mpk.py "$archive" --check-dir scratch/dump \
      >"$corpus_log" 2>&1; then
    exit 1
  fi
  grep -F "1 wrong size" "$corpus_log" >/dev/null || exit 1
) || fail=1
corpus_extra=scratch/dump/CODEX_VERIFY_EXTRA
(
  trap 'rm -f "$corpus_extra"' EXIT INT TERM
  : > "$corpus_extra"
  if python3 tools/asset/mpk.py "$archive" --check-dir scratch/dump \
      >"$corpus_log" 2>&1; then
    exit 1
  fi
  grep -F "9886 expected, 9887 present; 0 missing, 1 extra" \
    "$corpus_log" >/dev/null || exit 1
) || fail=1
echo "  one-missing, one-wrong-size, and one-extra negatives passed"

# A single-file RE query must not require inflating all 9,886 payload streams.
# Exercise the shipping path and the absent-entry discriminator: a silent empty
# result here would make every subsequent conclusion from an absent file false.
if [ -f "$archive" ]; then
  echo "=== MPK one-entry extraction ==="
  mkdir -p scratch/logs/mpk-entry-selftest
  mpk_one=scratch/logs/mpk-entry-selftest/sk1/M0001_00_00.lua
  mpk_log=scratch/logs/mpk-entry-selftest.log
  python3 tools/asset/mpk.py "$archive" \
    -o scratch/logs/mpk-entry-selftest -e sk1/M0001_00_00.lua 2>"$mpk_log" || fail=1
  grep -F "scanned 9886 entries, extracted 1: sk1/M0001_00_00.lua" "$mpk_log" || fail=1
  cmp -s "$mpk_one" scratch/dump/sk1/M0001_00_00.lua || fail=1
  if python3 tools/asset/mpk.py "$archive" \
      -o scratch/logs/mpk-entry-selftest -e sk1/DOES_NOT_EXIST.lua \
      >"$mpk_log" 2>&1; then
    fail=1
  fi
  grep -F "scanned 9886 entries, matched 0 for 'sk1/DOES_NOT_EXIST.lua'" \
    "$mpk_log" >/dev/null || fail=1
  echo "  missing-entry negative passed (9886 scanned, 0 matched)"
else
  echo "=== MPK one-entry extraction SKIPPED (shipping archive missing) ==="
fi

for t in stex smdl smot scol roomdata strings enemydat; do
  echo "=== $t ==="
  python3 "tools/asset/$t.py" || fail=1
done

# These four corpus parsers used to print FAILED while returning success, and
# also accepted an empty default glob. Exercise both negative classes through
# the shipping CLI so `python ... || fail=1` is evidence rather than decoration.
echo "=== parser failure propagation ==="
mkdir -p scratch/logs/parser-empty
: > scratch/logs/parser-malformed.bin
parser_root=$(pwd)
for t in stex smdl smot scol; do
  neg_log="scratch/logs/${t}-negative.log"
  if (cd scratch/logs/parser-empty && \
      python3 "$parser_root/tools/asset/$t.py") >"$neg_log" 2>&1; then
    fail=1
  fi
  grep -F "FATAL: scanned 0" "$neg_log" >/dev/null || fail=1
  if python3 "tools/asset/$t.py" scratch/logs/parser-malformed.bin \
      >"$neg_log" 2>&1; then
    fail=1
  fi
  grep -Ei '1 (files )?FAILED|0/1.*failed' "$neg_log" >/dev/null || fail=1
done
echo "  empty-corpus and malformed-input negatives passed for 4 parsers"
echo "=== combat self-test ==="
  ./build/mana --combat-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || fail=1

  echo "=== event-box self-test ==="
  ./build/mana --eventbox-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || fail=1
  ./build/mana --eventbox-selftest >/dev/null 2>&1 || fail=1

  # The first shipping map begins entirely inside Init(), after wait(600).
  # A Lua chunk that only parses, a frozen GetGameTimeMs stub, or an invented
  # boss handle all leave this log without the dialogue or the late-spawned
  # Jackal record.  Keep the proof in the real game path.
  echo "=== opening-room lifecycle ==="
  mkdir -p scratch/logs scratch/screenshots
  init_log=scratch/logs/opening-room-lifecycle.log
  SDL_AUDIODRIVER=dummy ./build/mana --screenshot scratch/screenshots/opening-room-lifecycle.png \
    --warmup 600 --auto-advance >"$init_log" 2>&1 || fail=1
  grep -F "started M0001_00_00 Init coroutine" "$init_log" || fail=1
  grep -F 'text] message: "Arena Guard:' "$init_log" || fail=1
  grep -F "enemy stats: 1 from enemydat.bin, 0 with no table entry" "$init_log" || fail=1
  grep -F "loaded late actor _BOSS model B0000_00" "$init_log" || fail=1
  grep -F "bgm 2:" "$init_log" || fail=1
  grep -F "scripted movement began for _BOSS" "$init_log" || fail=1

  # Move onto an authored EX_1 boundary cell. Jackal's shipping coroutine only
  # enables its charge there; the charge must damage the player and then stop
  # on the room's physical wall through ISHITMAP.
  echo "=== opening-boss attack path ==="
  boss_log=scratch/logs/opening-boss-attack.log
  SDL_AUDIODRIVER=dummy ./build/mana \
    --screenshot scratch/screenshots/opening-boss-attack.png \
    --warmup 600 --auto-advance --walk-to 30 30 >"$boss_log" 2>&1 || fail=1
  grep -E -- '-> [1-9][0-9]* landed hits' "$boss_log" || fail=1
  grep -E 'player took [1-9][0-9]* damage' "$boss_log" || fail=1
  grep -F "scripted map collision for _BOSS" "$boss_log" || fail=1

  echo "=== camera command self-test ==="
  ./build/mana --camera-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || fail=1
  ./build/mana --camera-selftest >/dev/null 2>&1 || fail=1

  echo "=== scripted movement self-test ==="
  ./build/mana --movement-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || fail=1
  ./build/mana --movement-selftest >/dev/null 2>&1 || fail=1

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

echo "=== cmd API ==="
python3 tools/asset/extract_cmd_api.py >/dev/null || fail=1
# Remove exactly one shipping registration while leaving the rest of the
# disassembly intact. The extractor must reject 199/200 rather than treating
# "some registrations found" as success.
cmd_api_negative=scratch/logs/cmd-api-one-missing.asm
sed '0,/tolua_function@plt/s//tolua_broken@plt/' \
  scratch/raw/full.asm >"$cmd_api_negative"
if python3 tools/asset/extract_cmd_api.py scratch/raw/libmcfandroid.so \
    "$cmd_api_negative" >scratch/logs/cmd-api-negative.log 2>&1; then
  fail=1
fi
grep -F "FATAL: cmd API incomplete: registrations 199/200" \
  scratch/logs/cmd-api-negative.log >/dev/null || fail=1
echo "  one-registration-missing negative passed (199/200 rejected)"

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

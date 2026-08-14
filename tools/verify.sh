#!/usr/bin/env bash
# Run every asset parser over the full extracted corpus.
# Fails loudly on an empty corpus rather than reporting a vacuous pass.
set -uo pipefail
cd "$(dirname "$0")/.."

# Verification is a batch workload: never create a desktop window or open an
# audio device. Gameplay scenarios additionally pass --no-audio directly (or
# inherit it from --opening-story), so a decoder regression cannot hide behind
# the dummy backend.
export SDL_VIDEODRIVER=offscreen
export SDL_AUDIODRIVER=dummy

echo "=== architecture structure gate ==="
python3 tools/check_structure.py --selftest || exit 1
python3 tools/check_structure.py || exit 1

echo "=== build shipping runtime ==="
cmake --build build --target mana mana_gpu_selftest -j2 || exit 1
mkdir -p scratch/logs

echo "=== SDL3 GPU offscreen device self-test ==="
./build/mana_gpu_selftest || exit 1
gpu_negative_log=scratch/logs/gpu-readback-negative.log
if ./build/mana_gpu_selftest --negative-control >"$gpu_negative_log" 2>&1; then
  echo "FATAL: SDL3 GPU readback negative control returned success"
  exit 1
fi
grep -F "magenta clear scanned 12 pixels, 12 mismatched" \
  "$gpu_negative_log" || exit 1
echo "  wrong-color negative passed (12/12 pixels rejected)"
gpu_pipeline_negative_log=scratch/logs/gpu-pipeline-negative.log
if ./build/mana_gpu_selftest --pipeline-negative-control \
    >"$gpu_pipeline_negative_log" 2>&1; then
  echo "FATAL: SDL3 GPU pipeline negative control returned success"
  exit 1
fi
grep -F "PIPELINE SELFTEST FAIL: scanned 48 pixels, 48 mismatched" \
  "$gpu_pipeline_negative_log" || exit 1
echo "  pipeline wrong-color negative passed (48/48 pixels rejected)"

echo "=== portable shader pack ==="
shader_regen=scratch/logs/shader-generated
OUTPUT_DIR="$shader_regen" ./tools/compile_shaders.sh || exit 1
python3 tools/embed_shader_pack.py --input-dir "$shader_regen" \
  --output scratch/logs/shader-pack.inc || exit 1
for shader in solid.vert.spv solid.vert.dxil solid.vert.msl \
              solid.frag.spv solid.frag.dxil solid.frag.msl; do
  cmp "$shader_regen/$shader" "shaders/generated/$shader" || exit 1
done
shader_pack_negative=scratch/logs/shader-pack-negative.log
if python3 tools/embed_shader_pack.py --input-dir scratch/logs/DOES_NOT_EXIST \
    --output scratch/logs/shader-pack-missing.inc >"$shader_pack_negative" 2>&1; then
  echo "FATAL: missing shader-pack negative returned success"
  exit 1
fi
grep -F "scanned 0 artifacts, expected 6; 6 missing" \
  "$shader_pack_negative" || exit 1
echo "  6/6 artifacts regenerate exactly; missing-pack negative passed"

check_runtime() {
  if [ ! -x "$1" ]; then
    echo "FATAL: game binary $1 is missing or not executable; 0 gameplay checks ran."
    return 1
  fi
  if [ ! -d "$2" ]; then
    echo "FATAL: runtime asset directory $2 is missing; 0 gameplay checks ran."
    return 1
  fi
  if [ ! -f "$3" ]; then
    echo "FATAL: source binary $3 is missing; 0 static extraction checks ran."
    return 1
  fi
}

check_runtime build/mana scratch/raw/assets scratch/raw/libmcfandroid.so || exit 1

archive=scratch/raw/assets/sk1/sk1.mpk
if [ ! -f "$archive" ]; then
  echo "FATAL: $archive is missing; corpus identity cannot be verified."
  echo "       A file count alone cannot prove scratch/dump came from this archive."
  exit 1
fi
python3 tools/asset/mpk.py "$archive" --check-dir scratch/dump || exit 1

fail=0
mark_failure() {
  echo "VERIFY CHECK FAILED at tools/verify.sh:${BASH_LINENO[0]}" >&2
  fail=1
}

  echo "=== runtime preflight negatives ==="
preflight_log=scratch/logs/runtime-preflight-negative.log
if check_runtime scratch/logs/DOES_NOT_EXIST scratch/raw/assets \
    scratch/raw/libmcfandroid.so \
    >"$preflight_log" 2>&1; then
  mark_failure
fi
grep -F "0 gameplay checks ran" "$preflight_log" >/dev/null || mark_failure
if check_runtime build/mana scratch/logs/DOES_NOT_EXIST \
    scratch/raw/libmcfandroid.so \
    >"$preflight_log" 2>&1; then
  mark_failure
fi
grep -F "0 gameplay checks ran" "$preflight_log" >/dev/null || mark_failure
if check_runtime build/mana scratch/raw/assets scratch/logs/DOES_NOT_EXIST \
    >"$preflight_log" 2>&1; then
  mark_failure
fi
grep -F "0 static extraction checks ran" "$preflight_log" >/dev/null || mark_failure
  echo "  missing game, assets, and source-binary negatives passed"

  stop_negative_log=scratch/logs/stop-room-negative.log
  if ./build/mana --room M0001_00_00 --stop-room M0099_99_99 \
      --screenshot scratch/screenshots/stop-room-negative.png --warmup 1 \
      --no-window --no-audio >"$stop_negative_log" 2>&1; then
    echo "FAIL: an unmet --stop-room returned success"
    mark_failure
  fi
  grep -F "requested stop room M0099_99_99 was not reached" \
    "$stop_negative_log" || mark_failure
  echo "  unmet stop-room negative passed"

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
) || mark_failure
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
) || mark_failure
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
    -o scratch/logs/mpk-entry-selftest -e sk1/M0001_00_00.lua 2>"$mpk_log" || mark_failure
  grep -F "scanned 9886 entries, extracted 1: sk1/M0001_00_00.lua" "$mpk_log" || mark_failure
  cmp -s "$mpk_one" scratch/dump/sk1/M0001_00_00.lua || mark_failure
  if python3 tools/asset/mpk.py "$archive" \
      -o scratch/logs/mpk-entry-selftest -e sk1/DOES_NOT_EXIST.lua \
      >"$mpk_log" 2>&1; then
    mark_failure
  fi
  grep -F "scanned 9886 entries, matched 0 for 'sk1/DOES_NOT_EXIST.lua'" \
    "$mpk_log" >/dev/null || mark_failure
  echo "  missing-entry negative passed (9886 scanned, 0 matched)"
else
  echo "=== MPK one-entry extraction SKIPPED (shipping archive missing) ==="
fi

for t in stex smdl smot scol roomdata strings enemydat; do
  echo "=== $t ==="
  python3 "tools/asset/$t.py" || mark_failure
done

# These four corpus parsers used to print FAILED while returning success, and
# also accepted an empty default glob. Exercise both negative classes through
# the shipping CLI so `python ... || mark_failure` is evidence rather than decoration.
echo "=== parser failure propagation ==="
mkdir -p scratch/logs/parser-empty
: > scratch/logs/parser-malformed.bin
parser_root=$(pwd)
for t in stex smdl smot scol; do
  neg_log="scratch/logs/${t}-negative.log"
  if (cd scratch/logs/parser-empty && \
      python3 "$parser_root/tools/asset/$t.py") >"$neg_log" 2>&1; then
    mark_failure
  fi
  grep -F "FATAL: scanned 0" "$neg_log" >/dev/null || mark_failure
  if python3 "tools/asset/$t.py" scratch/logs/parser-malformed.bin \
      >"$neg_log" 2>&1; then
    mark_failure
  fi
  grep -Ei '1 (files )?FAILED|0/1.*failed' "$neg_log" >/dev/null || mark_failure
done
echo "  empty-corpus and malformed-input negatives passed for 4 parsers"
echo "=== combat self-test ==="
  ./build/mana --combat-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || mark_failure

  echo "=== event-box self-test ==="
  ./build/mana --eventbox-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || mark_failure
  ./build/mana --eventbox-selftest >/dev/null 2>&1 || mark_failure

  # The first shipping map begins entirely inside Init(), after wait(600).
  # A Lua chunk that only parses, a frozen GetGameTimeMs stub, or an invented
  # boss handle all leave this log without the dialogue or the late-spawned
  # Jackal record.  Keep the proof in the real game path.
  echo "=== opening-room lifecycle ==="
  mkdir -p scratch/logs scratch/screenshots
  init_log=scratch/logs/opening-room-lifecycle.log
  ./build/mana --no-audio --screenshot scratch/screenshots/opening-room-lifecycle.png \
    --warmup 600 --auto-advance >"$init_log" 2>&1 || mark_failure
  grep -F "started M0001_00_00 Init coroutine" "$init_log" || mark_failure
  grep -F 'text] message: "Arena Guard:' "$init_log" || mark_failure
  grep -F "enemy stats: 1 from enemydat.bin, 0 with no table entry" "$init_log" || mark_failure
  grep -F "loaded late actor _BOSS model B0000_00" "$init_log" || mark_failure
  grep -F "scripted movement began for _BOSS" "$init_log" || mark_failure

  # Move onto an authored EX_1 boundary cell. Jackal's shipping coroutine only
  # enables its charge there; the charge must damage the player and then stop
  # on the room's physical wall through ISHITMAP.
  echo "=== opening-boss attack path ==="
  boss_log=scratch/logs/opening-boss-attack.log
  ./build/mana --no-audio \
    --screenshot scratch/screenshots/opening-boss-attack.png \
    --warmup 600 --auto-advance --walk-to 30 30 >"$boss_log" 2>&1 || mark_failure
  grep -E -- '-> [1-9][0-9]* landed hits' "$boss_log" || mark_failure
  grep -E 'player took [1-9][0-9]* damage' "$boss_log" || mark_failure
  grep -F "scripted map collision for _BOSS" "$boss_log" || mark_failure
  if grep -F "started EnemyDead coroutine" "$boss_log" >/dev/null; then mark_failure; fi

  # Complete the same authored fight. Auto attack waits for the EX_1 boundary,
  # closes through shipping collision, and swings through the normal player
  # volume. The last-hostile edge must start EnemyDead exactly once and the
  # room script must carry the game into Will's scene.
  echo "=== opening-boss death progression ==="
  death_log=scratch/logs/opening-boss-death.log
  ./build/mana --no-audio \
    --screenshot scratch/screenshots/opening-boss-death.png \
    --warmup 2200 --auto-advance --auto-attack --walk-to 30 30 \
    >"$death_log" 2>&1 || mark_failure
  grep -F "MainPlayer killed _BOSS" "$death_log" || mark_failure
  test "$(grep -Fc 'started EnemyDead coroutine' "$death_log")" -eq 1 || mark_failure
  grep -F "mapjump -> M0001_00_02" "$death_log" || mark_failure
  grep -F "started M0001_00_02 Init coroutine" "$death_log" || mark_failure
  grep -F 'text] message: "Sumo:\nDon'"'"'t die on me, Will!"' "$death_log" || mark_failure
  grep -E 'player attack tested [1-9][0-9]* volume pairs, produced [1-9][0-9]* geometric overlaps' \
    "$death_log" || mark_failure

  # Will's room has no event box or MapJump. Its north SetDoor(UP, FREE)
  # must traverse the world table on body contact, while the destination's
  # SetDoor(DN, KEY) must produce the opposite result under the same driver.
  echo "=== free and locked room doors ==="
  free_door_log=scratch/logs/free-door.log
  ./build/mana --no-audio --room M0001_00_02 --spawn 165 210 \
    --walk-to 165 -30 --auto-advance \
    --screenshot scratch/screenshots/free-door.png --warmup 300 \
    >"$free_door_log" 2>&1 || mark_failure
  grep -F "room exit 0 -> M0001_00_01" "$free_door_log" || mark_failure
  grep -F "ended in M0001_00_01" "$free_door_log" || mark_failure

  open_edge_log=scratch/logs/open-room-edge.log
  ./build/mana --no-audio --room M0001_00_01 --spawn 165 135 \
    --walk-to 165 -30 --screenshot scratch/screenshots/open-room-edge.png \
    --warmup 300 >"$open_edge_log" 2>&1 || mark_failure
  grep -F "room exit 0 -> M0001_00_00" "$open_edge_log" || mark_failure
  grep -F "ended in M0001_00_00" "$open_edge_log" || mark_failure

  key_door_log=scratch/logs/key-door.log
  ./build/mana --no-audio --room M0001_00_01 --spawn 165 60 \
    --walk-to 165 300 --screenshot scratch/screenshots/key-door.png --warmup 240 \
    >"$key_door_log" 2>&1 || mark_failure
  grep -F "ended in M0001_00_01" "$key_door_log" || mark_failure
  if grep -F "room exit" "$key_door_log" >/dev/null; then mark_failure; fi

  # Exercise the continuous authored story, not a room seeded with a test
  # scenario value: two distinct Jackal intros/deaths, Will's completed scene,
  # both ordinary/free room edges, and the post-fight out_01 callback. This
  # catches transient input locks leaking across MapJump and cutscene movement
  # escaping before sccnt is committed.
  echo "=== continuous opening story ==="
  story_log=scratch/logs/opening-story.log
  ./build/mana --opening-story \
    --screenshot scratch/screenshots/opening-story-fallback.png --warmup 5000 \
    >"$story_log" 2>&1 || mark_failure
  test "$(grep -Fc 'MainPlayer killed _BOSS' "$story_log")" -eq 2 || mark_failure
  grep -F 'message: "Arena Guard:\nDraw your blade, boy!' "$story_log" || mark_failure
  grep -F 'message: "Sumo:\nI guess it'"'"'s now or never!"' "$story_log" || mark_failure
  grep -F "entered event box 'out_01'" "$story_log" || mark_failure
  grep -F "scripted movement began for SHADOW" "$story_log" || mark_failure
  grep -F "room exit 1 -> M0001_01_03" "$story_log" || mark_failure
  grep -F "room exit 2 -> M0001_01_04" "$story_log" || mark_failure
  grep -F "room exit 3 -> M0001_00_04" "$story_log" || mark_failure
  grep -F "mapjump -> M0000_05_06 at (100,0,130)" "$story_log" || mark_failure
  grep -F 'message: "Sumo:\nI-I'"'"'m...alive! But where am I?"' "$story_log" || mark_failure
  grep -F "ended in M0000_05_06" "$story_log" || mark_failure
  grep -F "end state: sccnt=10, eventScene=0, cinema=false, player-control=true, 0 live coroutine(s)" \
    "$story_log" || mark_failure

  # Continue from the same unseeded new game through the actual overworld
  # route to Bogard. This is a distinct gate: it exercises stacked-floor
  # selection, all three vine levels, and the shipping in_01 callback while
  # remaining uncapped and silent through --opening-story.
  echo "=== continuous route to Bogard ==="
  bogard_log=scratch/logs/bogard-story.log
  ./build/mana --opening-story --continue-story --stop-room M0010_00_01 \
    --screenshot scratch/screenshots/bogard-story-fallback.png --warmup 4000 \
    >"$bogard_log" 2>&1 || mark_failure
  test "$(grep -Fc 'traversed event wall 1 -> 2' "$bogard_log")" -eq 3 || mark_failure
  grep -F "room exit 3 -> M0000_06_04" "$bogard_log" || mark_failure
  grep -F "room exit 2 -> M0000_06_05" "$bogard_log" || mark_failure
  grep -F "entered event box 'in_01'" "$bogard_log" || mark_failure
  grep -F "mapjump -> M0010_00_01 at (150,0,195) arrow 0" "$bogard_log" || mark_failure
  grep -F "reached requested stop room M0010_00_01" "$bogard_log" || mark_failure
  grep -F "audio decoded 0 sounds / 0 frames" "$bogard_log" || mark_failure

  # Continue back down the authored inverse vine route and through the heroine
  # encounter. This also guards AddEnemyZaco: its Init-time actors must be
  # created, engine-placed, seeded, defeated, and allowed to fire EnemyDead.
  # --opening-story makes the run fixed-step/uncapped, silent, and offscreen.
  echo "=== continuous route through heroine encounter ==="
  heroine_log=scratch/logs/heroine-story.log
  ./build/mana --opening-story --continue-story --stop-sccnt 12 \
    >"$heroine_log" 2>&1 || mark_failure
  # Three climbs outbound. The return has one descent in M0000_06_05, then two
  # climbs and the three-step eastern descent in M0000_07_05.
  test "$(grep -Fc 'traversed event wall 1 -> 2' "$heroine_log")" -eq 5 || mark_failure
  test "$(grep -Fc 'traversed event wall 2 -> 1' "$heroine_log")" -eq 4 || mark_failure
  test "$(grep -Fc 'engine-placed (script gave 0,0, extent 0)' "$heroine_log")" \
    -eq 3 || mark_failure
  grep -F "enemy stats: 3 from enemydat.bin, 0 with no table entry" \
    "$heroine_log" || mark_failure
  test "$(grep -Fc 'MainPlayer killed enemy3_' "$heroine_log")" -eq 3 || mark_failure
  grep -F "all live enemies defeated; started EnemyDead coroutine" \
    "$heroine_log" || mark_failure
  grep -F "reached settled scenario state sccnt=12" "$heroine_log" || mark_failure
  grep -F "end state: sccnt=12, eventScene=0, cinema=false, player-control=true, 0 live coroutine(s)" \
    "$heroine_log" || mark_failure
  grep -F "video driver: offscreen" "$heroine_log" || mark_failure
  grep -F "audio decoded 0 sounds / 0 frames" "$heroine_log" || mark_failure

  # The companion is persistent ModeGame state, not a room-local anonymous
  # spawn. Return over the authored route, restore PARTY_HEROINE after each
  # room load, talk to Bogard three times, and complete the pendant/Matock
  # objective without seeding sccnt. NPC_01 is a room-local script name reused
  # by unrelated actors, so the authored dialogue and settled state are the
  # discriminator rather than a global count of that name.
  echo "=== continuous heroine return to Bogard ==="
  bogard_heroine_log=scratch/logs/bogard-heroine-story.log
  ./build/mana --opening-story --continue-story --stop-sccnt 14 \
    >"$bogard_heroine_log" 2>&1 || mark_failure
  grep -F "restored PARTY_HEROINE after room load" "$bogard_heroine_log" || mark_failure
  grep -F 'message: "Wait. That pendant you'"'"'re wearing... Is that...?"' \
    "$bogard_heroine_log" || mark_failure
  grep -F 'message: "With a mattock in hand, a young lad of Sumo'"'"'s strength should make quick work of the rocks."' \
    "$bogard_heroine_log" || mark_failure
  grep -F "reached settled scenario state sccnt=14" "$bogard_heroine_log" || mark_failure
  grep -F "end state: sccnt=14, eventScene=0, cinema=false, player-control=true, 0 live coroutine(s)" \
    "$bogard_heroine_log" || mark_failure
  grep -F "party id 1 is PARTY_HEROINE alive" "$bogard_heroine_log" || mark_failure
  grep -F "video driver: offscreen" "$bogard_heroine_log" || mark_failure
  grep -F "audio decoded 0 sounds / 0 frames" "$bogard_heroine_log" || mark_failure

  echo "=== continuous Matock chest acquisition ==="
  matock_log=scratch/logs/matock-story.log
  ./build/mana --opening-story --continue-story --stop-item 17 \
    >"$matock_log" 2>&1 || mark_failure
  grep -F "room exit 0 -> M0010_00_00" "$matock_log" || mark_failure
  grep -F "opened box and acquired item 17" "$matock_log" || mark_failure
  grep -F "reached settled requested item 17" "$matock_log" || mark_failure
  grep -F "video driver: offscreen" "$matock_log" || mark_failure
  grep -F "audio decoded 0 sounds / 0 frames" "$matock_log" || mark_failure

  # Keep driving the same unseeded state after acquiring the Mattock. The
  # authored route must cross the cave without reversing through its arrival
  # box, descend the paired walls, climb Kett's physical steps, and execute the
  # bed scene through its real event box.
  echo "=== continuous post-Matock route through Kett scenario 15 ==="
  post_matock_log=scratch/logs/post-matock-story.log
  ./build/mana --opening-story --continue-story --stop-sccnt 15 \
    >"$post_matock_log" 2>&1 || mark_failure
  grep -F "opened box and acquired item 17" "$post_matock_log" || mark_failure
  grep -F "mapjump -> M0011_00_00" "$post_matock_log" || mark_failure
  test "$(grep -Fc "used Mattock weapon kind 6 on breakable object id 9" \
    "$post_matock_log")" -eq 2 || mark_failure
  grep -F "5 use(s) remain" "$post_matock_log" || mark_failure
  grep -F "room exit 2 -> M0011_00_01" "$post_matock_log" || mark_failure
  grep -F "room exit 2 -> M0011_00_02" "$post_matock_log" || mark_failure
  grep -F "completed authored post-Matock cave crossing through M0011_00_02/in_1" \
    "$post_matock_log" || mark_failure
  grep -F "mapjump -> M0012_01_01" "$post_matock_log" || mark_failure
  grep -F "entered event box 'bed_01'" "$post_matock_log" || mark_failure
  grep -F "You receive the Book of Curing." "$post_matock_log" || mark_failure
  grep -F "reached settled scenario state sccnt=15" "$post_matock_log" || mark_failure
  grep -F "video driver: offscreen" "$post_matock_log" || mark_failure
  grep -F "audio decoded 0 sounds / 0 frames" "$post_matock_log" || mark_failure

  # Keep driving the same unseeded state out of Kett Manor, through the
  # authored lizardman encounter, and back across the overworld to preserve
  # both equipped keys through the optional side room, use the generic
  # Keyring on the main route, operate the next pressure switch, and complete
  # the recovery spring.
  # This is also the discriminator for 3D auto-combat range, floor-aware
  # diagonal routing, and breakable-object item consumption.
  echo "=== continuous route through the Hydra recovery spring ==="
  silver_key_log=scratch/logs/silver-key-story.log
  ./build/mana --opening-story --continue-story --stop-sccnt 20 \
    >"$silver_key_log" 2>&1 || mark_failure
  grep -F "entered event box 'out_1'" "$silver_key_log" || mark_failure
  grep -F "mapjump -> M0000_10_09 at (165,0,135) arrow 2" \
    "$silver_key_log" || mark_failure
  grep -F "room exit 1 -> M0000_11_09" "$silver_key_log" || mark_failure
  grep -F "room exit 2 -> M0000_13_10" "$silver_key_log" || mark_failure
  test "$(grep -Fc 'MainPlayer killed enemy5_' "$silver_key_log")" -eq 5 || mark_failure
  grep -F "Warrior -> level 2" "$silver_key_log" || mark_failure
  grep -F "bought and equipped Keyring item 18 from Motie's authored shop" \
    "$silver_key_log" || mark_failure
  grep -F "equipped sub-item 30 in button slot 5" \
    "$silver_key_log" || mark_failure
  grep -F "opened box and acquired item 30" "$silver_key_log" || mark_failure
  grep -F "room exit 1 -> M0000_14_06" "$silver_key_log" || mark_failure
  grep -F "room exit 2 -> M0000_14_07" "$silver_key_log" || mark_failure
  grep -F "room exit 2 -> M0000_14_08" "$silver_key_log" || mark_failure
  grep -F "entered event box 'in_01'" "$silver_key_log" || mark_failure
  grep -F "mapjump -> M0013_03_01 at (225,0,45) arrow 3" \
    "$silver_key_log" || mark_failure
  grep -F "room exit 0 -> M0013_03_00" "$silver_key_log" || mark_failure
  grep -F "room exit 3 -> M0013_02_00" "$silver_key_log" || mark_failure
  grep -F "entered event box 'sw_01'" "$silver_key_log" || mark_failure
  grep -F "entered event box 'down_1'" "$silver_key_log" || mark_failure
  grep -F "mapjump -> M0013_00_04 at (75,0,75) arrow 2" \
    "$silver_key_log" || mark_failure
  grep -F "mapjump grounding at local (75.0,75.0): point floor=true (90.0); scanned 2 touchable boxes, 0 within body radius, 0 owned a collision floor; chose 90.0" \
    "$silver_key_log" || mark_failure
  grep -F "entered event box 'left_1'" "$silver_key_log" || mark_failure
  grep -F "mapjump -> M0013_02_00 at (75,0,45) arrow 1" \
    "$silver_key_log" || mark_failure
  grep -F "room exit 3 -> M0013_01_00" "$silver_key_log" || mark_failure
  grep -F "used Mattock weapon kind 6 on breakable object id 9 at (225.0,135.0); 4 use(s) remain" \
    "$silver_key_log" || mark_failure
  grep -F "entered event box 'down_01'" "$silver_key_log" || mark_failure
  grep -F "mapjump -> M0013_06_05 at (255,0,75) arrow 3" \
    "$silver_key_log" || mark_failure
  grep -F "entered lone WALL_UP volume at floor 330.0" \
    "$silver_key_log" || mark_failure
  grep -F "opening wall-plane route in M0013_06_05 to 'up_01'" \
    "$silver_key_log" || mark_failure
  grep -F "entered event box 'up_01'" "$silver_key_log" || mark_failure
  grep -F "mapjump -> M0013_01_00 at (225,0,45) arrow 3" \
    "$silver_key_log" || mark_failure
  grep -F "room exit 3 -> M0013_00_00" "$silver_key_log" || mark_failure
  grep -F "used equipped key item 18 from slot 4 to open room side 0" \
    "$silver_key_log" || mark_failure
  grep -F "room exit 0 -> M0013_02_01" "$silver_key_log" || mark_failure
  grep -F "entered event box 'sw_01'" "$silver_key_log" || mark_failure
  grep -F "mapjump -> M0013_11_00 at (225,0,45) arrow 3" \
    "$silver_key_log" || mark_failure
  grep -F "entered event box 'Recovery'" "$silver_key_log" || mark_failure
  grep -F "completed the authored Hydra recovery spring" \
    "$silver_key_log" || mark_failure
  grep -F "mapjump -> M0013_02_01 at (225,0,45) arrow 3" \
    "$silver_key_log" || mark_failure
  grep -F "room exit 2 -> M0013_00_02" "$silver_key_log" || mark_failure
  grep -F "video driver: offscreen" "$silver_key_log" || mark_failure
  grep -F "audio decoded 0 sounds / 0 frames" "$silver_key_log" || mark_failure

  # The semantic stop above continues that same unseeded state back out of the
  # recovery branch, down the authored wall plane, through the two-switch
  # hidden stair and Hydra's real EnemyDead transition, then opens both rewards,
  # runs AfterBossEvent, returns to Kett, reveals the Butler, and defeats the
  # shipping AddEnemy(123) Steward Wolf, then traverses Kett's hidden floor and
  # obtains the Chain Flail. Reuse its log: a second full-story run would only
  # duplicate evidence.
  echo "=== continuous Hydra return through Kett's Chain Flail ==="
  boss_cluster_log="$silver_key_log"
  grep -F "completed the authored Hydra recovery spring" \
    "$boss_cluster_log" || mark_failure
  grep -F "opening wall-plane route in M0013_06_05 from lone WALL_UP via 'wall_02b'" \
    "$boss_cluster_log" || mark_failure
  grep -F "entered event box 'wall_02b'" "$boss_cluster_log" || mark_failure
  grep -F "room exit 3 -> M0013_05_05" "$boss_cluster_log" || mark_failure
  grep -F "mapjump -> M0013_09_00 at (255,0,75) arrow 2" \
    "$boss_cluster_log" || mark_failure
  grep -F "used equipped key item 18 from slot 4 to open room side 1" \
    "$boss_cluster_log" || mark_failure
  grep -F "room exit 1 -> M0013_09_01" "$boss_cluster_log" || mark_failure
  grep -F "entered event box 'sw_01'" "$boss_cluster_log" || mark_failure
  grep -F "entered event box 'sw_02'" "$boss_cluster_log" || mark_failure
  grep -F "entered event box 'down_1'" "$boss_cluster_log" || mark_failure
  grep -F "mapjump -> M0013_08_04 at (75,0,45) arrow 2" \
    "$boss_cluster_log" || mark_failure
  if grep -F "opening route in M0013_08_04 reached waypoint" \
      "$boss_cluster_log" >/dev/null; then
    mark_failure
  fi
  grep -F "room exit 1 -> M0013_09_04" "$boss_cluster_log" || mark_failure
  grep -F "loaded late actor _BOSS model B0001_00" \
    "$boss_cluster_log" || mark_failure
  grep -F "MainPlayer killed _BOSS" "$boss_cluster_log" || mark_failure
  grep -F 'message: "\nYou slay the Hydra!"' "$boss_cluster_log" || mark_failure
  grep -F "opened box and acquired item 505" \
    "$boss_cluster_log" || mark_failure
  grep -F "equipped sub-item 31 in button slot 6" \
    "$boss_cluster_log" || mark_failure
  grep -F "opened box and acquired item 31" \
    "$boss_cluster_log" || mark_failure
  grep -F "mapjump -> M0000_14_08 at (165,0,75) arrow 2" \
    "$boss_cluster_log" || mark_failure
  grep -F "room exit 2 -> M0000_14_09" "$boss_cluster_log" || mark_failure
  grep -F "room exit 3 -> M0000_10_09" "$boss_cluster_log" || mark_failure
  grep -F "mapjump -> M0012_01_01 at (150,0,205) arrow 0" \
    "$boss_cluster_log" || mark_failure
  grep -F "loaded late actor enemy123_1 model B0023_00" \
    "$boss_cluster_log" || mark_failure
  grep -F "MainPlayer killed enemy123_1" "$boss_cluster_log" || mark_failure
  grep -F "mapjump -> M0012_06_00 at (225,0,45) arrow 3" \
    "$boss_cluster_log" || mark_failure
  grep -F "entered event box 'sw_1'" "$boss_cluster_log" || mark_failure
  grep -F "mapjump -> M0012_10_02 at (225,0,195) arrow 3" \
    "$boss_cluster_log" || mark_failure
  grep -F "used equipped key item 18 from slot 4 to open room side 0" \
    "$boss_cluster_log" || mark_failure
  grep -F "room exit 0 -> M0012_11_00" "$boss_cluster_log" || mark_failure
  grep -F "opened box and acquired item 104" "$boss_cluster_log" || mark_failure
  grep -F "reached settled scenario state sccnt=20" \
    "$boss_cluster_log" || mark_failure
  grep -F "video driver: offscreen" "$boss_cluster_log" || mark_failure
  grep -F "audio decoded 0 sounds / 0 frames" "$boss_cluster_log" || mark_failure

  # A locked door with no accepted equipped key used to spin forever after
  # its route deque became empty. Prove the shipping artifact emits a bounded,
  # detailed negative and exits nonzero for that exact class.
  echo "=== locked-door headless negative ==="
  locked_door_log=scratch/logs/locked-door-negative.log
  if ./build/mana --opening-story --room M0013_08_01 \
      --walk-to 360 135 --stop-room M0099_99_99 \
      >"$locked_door_log" 2>&1; then
    echo "FAIL: keyless locked-door route returned success"
    mark_failure
  fi
  grep -F "opening route blocked for 120 frames at locked room side 1 in M0013_08_01; accepted key ids are 18/30/37, equipped item buttons are [0,0,0,0]" \
    "$locked_door_log" || mark_failure
  grep -F "audio decoded 0 sounds / 0 frames" "$locked_door_log" || mark_failure

  echo "=== camera command self-test ==="
  ./build/mana --camera-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || mark_failure
  ./build/mana --camera-selftest >/dev/null 2>&1 || mark_failure

  echo "=== scripted movement self-test ==="
  ./build/mana --movement-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || mark_failure
  ./build/mana --movement-selftest >/dev/null 2>&1 || mark_failure

  echo "=== player self-test ==="
  ./build/mana --player-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || mark_failure
  ./build/mana --player-selftest >/dev/null 2>&1 || mark_failure

  echo "=== inventory self-test ==="
  ./build/mana --inventory-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || mark_failure
  ./build/mana --ai-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || mark_failure
  ./build/mana --ai-selftest >/dev/null 2>&1 || mark_failure
  ./build/mana --mode-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || mark_failure
  ./build/mana --mode-selftest >/dev/null 2>&1 || mark_failure
  ./build/mana --png-selftest 2>&1 | grep -E "SELFTEST:|FAIL" || mark_failure
  ./build/mana --png-selftest >/dev/null 2>&1 || mark_failure
  ./build/mana --inventory-selftest >/dev/null 2>&1 || mark_failure

  echo "=== text self-test ==="
  for l in en ja; do
    ./build/mana --text-selftest --lang "$l" 2>&1 | grep -E "SELFTEST:|sweep" || mark_failure
    ./build/mana --text-selftest --lang "$l" >/dev/null 2>&1 || mark_failure
  done

  # Loads every room in the game headlessly. Non-zero on a mesh/script failure
  # or an unresolved object id.
  echo "=== room census ==="
  ./build/mana --no-audio --room-census 2>&1 | grep "^\[census\]" || mark_failure
  ./build/mana --no-audio --room-census >/dev/null 2>&1 || mark_failure

echo "=== cmd API ==="
python3 tools/asset/extract_cmd_api.py >/dev/null || mark_failure

echo "=== RE frontier ==="
python3 tools/re_frontier_check.py || mark_failure
frontier_empty=scratch/logs/re-frontier-empty.md
sed '/^### /,$d' docs/re-frontier.md >"$frontier_empty"
if python3 tools/re_frontier_check.py "$frontier_empty" \
    >scratch/logs/re-frontier-empty.log 2>&1; then
  mark_failure
fi
grep -F "RE-FRONTIER FAIL: parsed 0 structured entries" \
  scratch/logs/re-frontier-empty.log >/dev/null || mark_failure
echo "  zero-entry negative passed (empty parse rejected)"
# Remove exactly one shipping registration while leaving the rest of the
# disassembly intact. The extractor must reject 199/200 rather than treating
# "some registrations found" as success.
cmd_api_negative=scratch/logs/cmd-api-one-missing.asm
sed '0,/tolua_function@plt/s//tolua_broken@plt/' \
  scratch/raw/full.asm >"$cmd_api_negative"
if python3 tools/asset/extract_cmd_api.py scratch/raw/libmcfandroid.so \
    "$cmd_api_negative" >scratch/logs/cmd-api-negative.log 2>&1; then
  mark_failure
fi
grep -F "FATAL: cmd API incomplete: registrations 199/200" \
  scratch/logs/cmd-api-negative.log >/dev/null || mark_failure
echo "  one-registration-missing negative passed (199/200 rejected)"

# Generate derived tables under scratch, never over tracked files. Verification
# is read-only: compare every generated byte with the checked-in artifact.
  generated=scratch/logs/generated-tables
  mkdir -p "$generated/docs" "$generated/src/engine"
  rm -f "$generated/docs/object-table.md" \
        "$generated/src/engine/object_table.inc" \
        "$generated/docs/weapon-table.md" \
        "$generated/src/engine/weapon_table.inc" \
        "$generated/docs/item-table.md" \
        "$generated/src/engine/item_uses.inc" \
        "$generated/src/engine/item_prices.inc"
  echo "=== map-object table ==="
  (cd "$generated" && python3 "$parser_root/tools/asset/object_table.py" \
    "$parser_root/scratch/raw/libmcfandroid.so" "$parser_root/scratch/dump/sk1") || mark_failure
  cmp -s "$generated/docs/object-table.md" docs/object-table.md || mark_failure
  cmp -s "$generated/src/engine/object_table.inc" \
    src/engine/object_table.inc || mark_failure
  echo "=== weapon table ==="
  (cd "$generated" && python3 "$parser_root/tools/asset/weapon_table.py" \
    "$parser_root/scratch/raw/libmcfandroid.so") || mark_failure
  cmp -s "$generated/docs/weapon-table.md" docs/weapon-table.md || mark_failure
  cmp -s "$generated/src/engine/weapon_table.inc" \
    src/engine/weapon_table.inc || mark_failure
  echo "=== item table ==="
  (cd "$generated" && python3 "$parser_root/tools/asset/item_table.py" \
    "$parser_root/scratch/raw/libmcfandroid.so" \
    "$parser_root/scratch/dump/sk1/str_en.bin") || mark_failure
  cmp -s "$generated/docs/item-table.md" docs/item-table.md || mark_failure
  cmp -s "$generated/src/engine/item_uses.inc" \
    src/engine/item_uses.inc || mark_failure
  cmp -s "$generated/src/engine/item_prices.inc" \
    src/engine/item_prices.inc || mark_failure
  cp "$generated/docs/item-table.md" "$generated/docs/item-table-mismatch.md"
  printf '\nMISMATCH\n' >> "$generated/docs/item-table-mismatch.md"
  if cmp -s "$generated/docs/item-table-mismatch.md" docs/item-table.md; then
    mark_failure
  fi
  echo "  7 generated artifacts match tracked bytes; mismatch negative passed"
  echo "=== world map ==="
  python3 tools/asset/worldmap.py --check || mark_failure

[ "$fail" -eq 0 ] && echo "ALL PARSERS PASSED" || echo "FAILURES ABOVE"
exit "$fail"

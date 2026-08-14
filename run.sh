#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

if [ ! -f build/build.ninja ] && [ ! -f build/Makefile ]; then
  echo "run.sh: build is not configured; run: cmake -S . -B build -G Ninja" >&2
  exit 1
fi

asset_root="${MANA_ASSETS:-scratch/raw/assets}"
if [ ! -f "$asset_root/sk1/sk1.mpk" ]; then
  echo "run.sh: required shipping archive is missing: $asset_root/sk1/sk1.mpk" >&2
  exit 1
fi

cmake --build build --target mana -j2
exec ./build/mana "$@"

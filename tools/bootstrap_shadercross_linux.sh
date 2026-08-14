#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

run_id=28236415347
artifact=SDL3_shadercross-linux-x64
archive=SDL3_shadercross-3.0.0-linux-x64.tar.gz
expected_sha256=252de380a0a4c6b5479419be3e4f00e419805fc99a41bae840096f0708ce3e15
root=scratch/tools/shadercross
binary="$root/SDL3_shadercross-3.0.0-linux-x64/bin/shadercross"
library="$root/SDL3_shadercross-3.0.0-linux-x64/lib"

if [ ! -x "$binary" ]; then
  command -v gh >/dev/null || {
    echo "FATAL: gh is required to download SDL's official Shadercross artifact." >&2
    exit 2
  }
  mkdir -p "$root"
  if [ ! -f "$root/$archive" ]; then
    gh run download "$run_id" -R libsdl-org/SDL_shadercross \
      -n "$artifact" -D "$root"
  fi
  actual_sha256=$(sha256sum "$root/$archive" | cut -d' ' -f1)
  if [ "$actual_sha256" != "$expected_sha256" ]; then
    echo "FATAL: Shadercross artifact SHA-256 is $actual_sha256, expected $expected_sha256" >&2
    exit 2
  fi
  tar -xzf "$root/$archive" -C "$root"
fi

if [ ! -x "$binary" ]; then
  echo "FATAL: downloaded artifact contains no executable Shadercross CLI." >&2
  exit 2
fi
LD_LIBRARY_PATH="$library${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  "$binary" --help >/dev/null 2>&1
echo "Shadercross ready: official run $run_id, artifact $artifact"

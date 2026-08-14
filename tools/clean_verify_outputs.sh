#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

case "${1:-}" in
  restore-corpus-member)
    if [ -f scratch/logs/mpk-corpus-held.lua ]; then
      rm -f scratch/dump/sk1/M0001_00_00.lua
      mv scratch/logs/mpk-corpus-held.lua \
        scratch/dump/sk1/M0001_00_00.lua
    fi
    ;;
  remove-corpus-extra)
    rm -f scratch/dump/CODEX_VERIFY_EXTRA
    ;;
  generated-tables)
    rm -f \
      scratch/logs/generated-tables/docs/object-table.md \
      scratch/logs/generated-tables/src/engine/object_table.inc \
      scratch/logs/generated-tables/docs/weapon-table.md \
      scratch/logs/generated-tables/src/engine/weapon_table.inc \
      scratch/logs/generated-tables/docs/item-table.md \
      scratch/logs/generated-tables/src/engine/item_uses.inc \
      scratch/logs/generated-tables/src/engine/item_prices.inc
    ;;
  *)
    echo "usage: $0 {restore-corpus-member|remove-corpus-extra|generated-tables}" >&2
    exit 2
    ;;
esac

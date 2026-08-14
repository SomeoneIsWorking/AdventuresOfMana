#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

shadercross=${SHADERCROSS:-scratch/tools/shadercross/SDL3_shadercross-3.0.0-linux-x64/bin/shadercross}
shadercross_lib=${SHADERCROSS_LIB:-scratch/tools/shadercross/SDL3_shadercross-3.0.0-linux-x64/lib}
output_dir=${OUTPUT_DIR:-shaders/generated}
if [ ! -x "$shadercross" ]; then
  echo "FATAL: Shadercross is missing at $shadercross; 0 shaders generated." >&2
  echo "Run tools/bootstrap_shadercross_linux.sh or set SHADERCROSS and " \
       "SHADERCROSS_LIB to an official SDL_shadercross build." >&2
  exit 2
fi

mkdir -p "$output_dir"
for source_name in solid.vert solid.frag overlay.vert overlay.frag \
    textured.vert textured.frag skinned.vert ui.vert ui.frag \
    sprite.vert sprite.frag; do
  program=${source_name%.*}
  stage=${source_name#*.}
  case "$stage" in
    vert) shader_stage=vertex ;;
    frag) shader_stage=fragment ;;
  esac
  for format in SPIRV DXIL MSL; do
    case "$format" in
      SPIRV) extension=spv ;;
      DXIL) extension=dxil ;;
      MSL) extension=msl ;;
    esac
    LD_LIBRARY_PATH="$shadercross_lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
      "$shadercross" "shaders/src/$source_name.hlsl" \
      --source HLSL --dest "$format" --stage "$shader_stage" \
      --entrypoint main --output "$output_dir/$source_name.$extension"
    if [ "$format" = MSL ]; then
      python3 tools/normalize_shader_text.py \
        "$output_dir/$source_name.$extension"
    fi
  done
done

echo "generated 33 shader artifacts from 11 HLSL sources"

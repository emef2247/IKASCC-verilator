#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
RTL_DIR="$ROOT_DIR/rtl/src"
SRC_DIR="$ROOT_DIR/src"
mkdir -p "$BUILD_DIR"
command -v verilator >/dev/null || { echo 'Verilator is required.' >&2; exit 1; }
[[ -f "$RTL_DIR/IKASCC.v" ]] || { echo 'Prepare rtl/ with tools/create_rtl_dir.sh first.' >&2; exit 1; }
RTL_SOURCES=("$RTL_DIR/IKASCC.v")
while IFS= read -r -d '' file; do RTL_SOURCES+=("$file"); done < <(find "$RTL_DIR/IKASCC_modules" -type f -name '*.v' -print0 | sort -z)
python3 "$ROOT_DIR/tools/prepare_rtl.py" --check
cd "$BUILD_DIR"
verilator --cc --exe --trace --top-module IKASCC \
  -GIMPL_TYPE=1 -GRAM_BLOCK=0 \
  -Wno-fatal -Wno-WIDTH -Wno-DECLFILENAME \
  "${RTL_SOURCES[@]}" "$SRC_DIR/ikascc_wrapper.cpp" "$SRC_DIR/scc_bus.c" \
  "$SRC_DIR/vgm_parser.c" "$SRC_DIR/vgm_player.c" "$SRC_DIR/main_vgm_csv.c" "$SRC_DIR/wav_writer.c" \
  -CFLAGS '-O3 -std=c++11' -LDFLAGS '-lz' -o ikascc_sim
make -C obj_dir -f VIKASCC.mk -j2 ikascc_sim
if [[ "${1:-}" == '--build-only' ]]; then exit 0; fi
cd "$ROOT_DIR"
if [[ $# -eq 0 ]]; then set -- tests/vgm/scc_scale.vgm; fi
"$BUILD_DIR/obj_dir/ikascc_sim" "$@"

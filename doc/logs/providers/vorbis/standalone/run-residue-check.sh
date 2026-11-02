#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "Usage: bash $0 MULTIMEDIA_SOURCE_ROOT MULTIMEDIA_BUILD_DIR OUTPUT_DIR" >&2
  exit 2
fi

source_root=$(realpath "$1")
build=$(realpath "$2")/stormbyte-buildmaster/bm
mkdir -p "$3"
output=$(realpath "$3")
compiler=${CXX:-clang++-22}
export LD_LIBRARY_PATH="$("$compiler" --print-runtime-dir)${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
program=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/vorbis-residue-leak-check.cxx
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

if "$compiler" -std=c++17 -fsanitize=address,undefined -shared-libasan \
    -fno-omit-frame-pointer \
    -I"$source_root/thirdparty/ffmpeg/plugins/codecs/audio/vorbis/src/include" \
    -I"$build/ogg/include" "$program" \
    "$build/vorbis/lib/libvorbisenc.a" "$build/vorbis/lib/libvorbis.a" \
    "$build/ogg/libogg.a" -lm -o "$output/vorbis-residue-leak-check" \
    > "$output/compile.log" 2>&1; then
  echo "exit_code: 0" >> "$output/compile.log"
else
  result=$?
  echo "exit_code: $result" >> "$output/compile.log"
  cat "$output/compile.log"
  exit "$result"
fi

for mode in minimal matrix; do
  args=()
  [[ $mode == matrix ]] && args=(matrix)
  if "$output/vorbis-residue-leak-check" "${args[@]}" > "$output/$mode.log" 2>&1; then
    echo "exit_code: 0" >> "$output/$mode.log"
  else
    result=$?
    echo "exit_code: $result" >> "$output/$mode.log"
    cat "$output/$mode.log"
    exit "$result"
  fi
  cat "$output/$mode.log"
done

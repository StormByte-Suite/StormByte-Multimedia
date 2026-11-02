#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "Usage: bash $0 {openh264|fdk-aac|kvazaar} PATCHED_SOURCE_ROOT OUTPUT_DIR" >&2
  exit 2
fi

provider=$1
source_root=$(realpath "$2")
mkdir -p "$3"
output=$(realpath "$3")
evidence=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
compiler=${CXX:-clang++-22}
flags=(-std=c++17 -fsanitize=address,undefined -fno-omit-frame-pointer)
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

case "$provider" in
  openh264)
    programs=(openh264-bitwriter-check)
    includes=(-I"$source_root/codec/common/inc" -I"$source_root/codec/api/wels")
    ;;
  fdk-aac)
    programs=(fdk-fixed-multiply-check fdk-scale-check)
    includes=(-I"$source_root/libFDK/include" -I"$source_root/libSYS/include")
    ;;
  kvazaar)
    programs=(kvazaar-sad-check)
    includes=(-I"$source_root/src")
    flags+=(-msse4.1)
    ;;
  *)
    echo "Unknown provider: $provider" >&2
    exit 2
    ;;
esac

for program in "${programs[@]}"; do
  log="$output/$program.log"
  {
    printf 'Compiler: '
    "$compiler" --version
    printf 'Source root: %s\n' "$source_root"
    printf 'Command:'
    printf ' %q' "$compiler" "${flags[@]}" "${includes[@]}" \
      "$evidence/providers/$provider/standalone/$program.cxx" -o "$output/$program"
    printf '\n'
  } > "$log"
  if "$compiler" "${flags[@]}" "${includes[@]}" \
      "$evidence/providers/$provider/standalone/$program.cxx" \
      -o "$output/$program" >> "$log" 2>&1; then
    echo "compile exit_code: 0" >> "$log"
  else
    result=$?
    echo "compile exit_code: $result" >> "$log"
    cat "$log"
    exit "$result"
  fi
  if "$output/$program" >> "$log" 2>&1; then
    echo "run exit_code: 0" >> "$log"
  else
    result=$?
    echo "run exit_code: $result" >> "$log"
    cat "$log"
    exit "$result"
  fi
  cat "$log"
done

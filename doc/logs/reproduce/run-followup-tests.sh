#!/usr/bin/env bash
set -uo pipefail

if [[ $# != 3 ]]; then
    echo "Usage: $0 BUILD_DIR {providers|svt|remaining|preview|deadlines} OUTPUT_DIR" >&2
    exit 2
fi

root=$(cd "$(dirname "$0")/../../.." && pwd)
build=$(realpath "$1")
scope=$2
mkdir -p "$3"
output=$(realpath "$3")
export LD_LIBRARY_PATH="$(clang-22 --print-runtime-dir):/usr/lib/llvm/22/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
unset LD_PRELOAD LSAN_OPTIONS
jobs=4

case "$scope" in
    providers)
        regex="^($(paste -sd'|' "$root/doc/logs/runs/followup/failed-paths-selection.txt"))$"
        ;;
    svt)
        regex='^pipeline.test_(svt_av1_cancel_repeated|interleave_encoded_video_and_audio|interleave_encoded_video_remuxed_audio|interleave_encoded_video_loudnorm_audio)$'
        ;;
    remaining)
        regex="^($(grep -v '^pipeline.test_libvorbis_encodes_surround_matroska$' "$root/doc/logs/runs/followup/remaining-selection.txt" | paste -sd'|'))$"
        ;;
    preview)
        jobs=1
        regex='^preview.test_(av_seek_switch_epochs|hdr_preview_tone_mapping|hdr_preview_preserves_signal|live_audio_selection|live_audio_tracks|subtitle_decode|faulted_reader_retained)$'
        ;;
    deadlines)
        jobs=2
        regex='^(file.test_progress_duration_hint_skips_scan|preview.test_(av_seek_switch_epochs|hdr_preview_tone_mapping|hdr_preview_preserves_signal|live_audio_selection|live_audio_tracks|subtitle_decode|faulted_reader_retained))$'
        ;;
    *)
        echo "Unknown scope: $scope" >&2
        exit 2
        ;;
esac

if [[ "$scope" != providers ]]; then
    export LSAN_OPTIONS="suppressions=$root/doc/logs/reproduce/nvidia-driver.lsan:print_suppressions=1"
fi
{
    printf 'ASAN_OPTIONS=%s\nUBSAN_OPTIONS=%s\nLSAN_OPTIONS=%s\n' \
        "$ASAN_OPTIONS" "$UBSAN_OPTIONS" "${LSAN_OPTIONS-unset}"
    printf 'ctest --test-dir %q -j%s -R %q --output-on-failure --output-junit %q\n' \
        "$build/test" "$jobs" "$regex" "$output/results.xml"
} > "$output/command.log"
ctest --test-dir "$build/test" -j"$jobs" -R "$regex" --output-on-failure \
    --output-junit "$output/results.xml" > "$output/console.log" 2>&1
result=$?
cp "$build/test/Testing/Temporary/LastTest.log" "$output/LastTest.log" || exit 1
printf 'exit_code=%s\n' "$result" | tee -a "$output/command.log"
exit "$result"

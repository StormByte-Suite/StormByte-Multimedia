#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: bash $0 MULTIMEDIA_BUILD_DIR OUTPUT_DIR" >&2
  exit 2
fi

build=$(realpath "$1")
mkdir -p "$2"
output=$(realpath "$2")
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
tests=(
  test_vmaf_picture_api_contract
  test_transcoder_vmaf_remux_matches_source_at_100
  test_openh264_encodes_h264_matroska
  test_openh264_encodes_ten_bit_after_format
  test_software_decode_x264_encodes_h264_matroska
  test_mp3_tags_roundtrip_through_opus
  test_audio_cover_mp3
  test_transcoder_kvazaar_pads_unaligned_video
  test_transcoder_hevc_encode_output_file_properties
  test_reencode_svt_av1_dovi_only
  test_transcoder_svt_av1_encode_preserves_hdr10_metadata
  test_opus_tags_preserved_in_vorbis
  test_fdk_aac_decodes_surround_to_flac
  test_fdk_aac_encodes_stereo_mp4
)
selection=$(IFS='|'; echo "${tests[*]}")
if ctest --test-dir "$build/test" -j4 --output-on-failure \
    --output-junit "$output/provider-tests.xml" \
    -R "^pipeline\\.($selection)$" > "$output/provider-tests.log" 2>&1; then
  result=0
else
  result=$?
fi
printf 'exit_code: %s\n' "$result" > "$output/exit-code.txt"
cat "$output/provider-tests.log"
exit "$result"

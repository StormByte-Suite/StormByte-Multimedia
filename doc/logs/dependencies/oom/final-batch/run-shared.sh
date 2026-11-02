#!/usr/bin/env bash
set -u
root="$(cd "$(dirname "$0")/../../../../.." && pwd)"
build="${BUILD_DIR:-/tmp/BMDATA/sanitizer-clean-final}"
evidence="$root/doc/logs/dependencies/oom/final-batch"
cd "$root" || exit 1
unset LD_PRELOAD
export LD_LIBRARY_PATH="$(clang-22 --print-runtime-dir):/usr/lib/llvm/22/lib64"
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
export LSAN_OPTIONS="suppressions=$root/doc/logs/reproduce/nvidia-driver.lsan:print_suppressions=1"
selection='pipeline.test_(transcoder_plan_allocation_failure|transcoder_empty_plan_owner|pipeline_failure_closure_without_allocations|nlmeans_scratch_failure_fails_job|transcoder_start_without_allocations|transcoder_worker_without_allocations|transcoder_cancel_without_allocations|writer_destruction_without_allocations|stage_factory_allocation_failure|filter_report_allocation_failure|subtitle_allocation_failure|stage_wiring_allocation_failure|filter_process_allocation_failure|track_configuration_allocation_failure|filter_graph_delayed_output_and_eof|filter_count_frames|filter_psnr|filter_ssim|filter_xpsnr|filter_adeclick|filter_loudnorm|filter_upmix|filter_degrain|filter_watermark|filter_nlmeans|transcoder_vmaf_remux_matches_source_at_100|srt_encodes_srt|ass_encodes_matroska|transcoder_remuxes_mp4_audio_into_matroska)$'
ctest --test-dir "$build/test" --output-on-failure --no-tests=error \
    --timeout 180 -j2 -R "$selection" > "$evidence/shared-ctest.log" 2>&1
rc=$?
printf '\nexit_code=%s\n' "$rc" >> "$evidence/shared-ctest.log"
cp "$build/test/Testing/Temporary/LastTest.log" "$evidence/shared-LastTest.log"
tail -35 "$evidence/shared-ctest.log"
exit "$rc"

#!/usr/bin/env bash
set -u
root="$(cd "$(dirname "$0")/../../../../.." && pwd)"
cd "$root" || exit 1
unset LD_PRELOAD ASAN_OPTIONS LSAN_OPTIONS
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
export LD_LIBRARY_PATH="$root/build/oom-validation:$root/build/oom-validation/base-plain/lib:$(clang-22 --print-runtime-dir):/usr/lib/llvm/22/lib64"
evidence="$root/doc/logs/dependencies/oom/final-batch"
failed=0
for test in \
    test_pipeline_failure_closure_without_allocations \
    test_nlmeans_scratch_failure_fails_job \
    test_transcoder_start_without_allocations \
    test_transcoder_worker_without_allocations \
    test_transcoder_cancel_without_allocations \
    test_writer_destruction_without_allocations \
    test_stage_factory_allocation_failure \
    test_filter_report_allocation_failure \
    test_subtitle_allocation_failure \
    test_stage_wiring_allocation_failure \
    test_filter_process_allocation_failure \
    test_track_configuration_allocation_failure
do
    timeout 90 build/oom-validation/persistent-tests "$test" \
        > "$evidence/final-$test.log" 2>&1
    rc=$?
    printf '\nexit_code=%s\n' "$rc" >> "$evidence/final-$test.log"
    printf '%s exit_code=%s\n' "$test" "$rc"
    if [ "$rc" -ne 0 ]; then
        failed=1
    fi
done
exit "$failed"

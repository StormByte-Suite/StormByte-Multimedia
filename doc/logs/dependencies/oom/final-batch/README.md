# Final allocation-boundary batch

This follow-up covers allocating FFmpeg/OCR owners, pipeline configuration and
wiring, filter lifecycle callbacks, and private workers. It is not a full-suite
rerun or a release authorization. No versions, tags or SONAMEs were changed.

## Latest verified results

Final code revisions:

- `88b9f418`: FFmpeg/OCR allocation propagation and RAII.
- `3a8a80b6`: backend worker/queue failure boundaries and cleanup.
- `a46701f6`: filter lifecycle guards and helper propagation.
- `08a0461d`: public pipeline/configuration/wiring and three new regressions.
- `a354f199`: matching private scoped-logger declaration.

The combined working-tree source was validated before being split into these
commits; intermediate commits are not claimed to be independently build-tested.
Every code commit has its own changelog entry. No Multimedia push was performed.

| Check | Result | Evidence |
|---|---|---|
| Shared library and affected test executables, incremental build | PASS, exit 0 | [build log](../../../builds/oom-batch-final-incremental.log) |
| Isolated UBSan build, including committed constructor prerequisites | PASS, exit 0 | [build log](isolated-final-build.log) |
| Twelve persistent allocator-injection regressions, final sources | 12 PASS, exit 0 each | `final-test_*.log` in this directory |
| Shared strict ASan/UBSan/LSan selection | 17 PASS, 12 intentional SKIP, exit 0 | [CTest output](shared-ctest.log), [full test output](shared-LastTest.log) |
| Shared strict sanitizer selection after the last source fixes | 9 PASS, no SKIP, exit 0 | [CTest output](shared-final-delta.log), [full test output](shared-final-delta-LastTest.log) |
| Targeted clang-tidy exception checks, 90 changed translation units | 0 analyzer execution/compilation failures; warnings remain | [results and per-unit logs](../../../static-analysis/final/oom-batch/) |

The nine delta cases overlap the earlier seventeen passes; these are not
twenty-six distinct passing cases. The twelve shared skips use glibc allocator
interposition, which is deliberately disabled under ASan. All twelve actually
execute in the isolated UBSan run. Skips are not passes.

The twelve final cases cover failure closure, NLMeans scratch exhaustion,
coordinator startup/worker/cancellation, real writer destruction, stage factories,
report snapshots, subtitles, stage wiring, filter processing and track
configuration. Each `final-test_*.log` records its actual exit code.

The initial factory abort exposed a stale StageTelemetry object in the isolated
build: rebuilding only uncommitted sources omitted an already committed
constructor fix. The isolated build now includes sources changed since
`6a08069a`. The track-configuration abort exposed a real nested `JobLog`
noexcept barrier, which was removed. Original bounded backtraces remain here;
the `final-` logs supersede the earlier results rather than erasing them.

## Commands and reproducibility

[run-shared.sh](run-shared.sh) preserves the 29-case selection and strict
sanitizer environment. The actual CTest root is the build's `test` subdirectory;
`--no-tests=error` prevents an empty selection being accepted.
The [isolated runner](run-isolated.sh) preserves all twelve final case names,
timeouts and exit-code handling. [build-current.py](build-current.py) is the
exact local build recipe used with the existing UBSan compilation database
and dependency prefix, not a standalone fresh-machine bootstrap script.
The CTest NLMeans case took 263.56 seconds, using its configured per-test
timeout; it was not hung.

The allocator interposer, test main and assertions are checked into
[allocation_failure_test.cxx](../../../../../test/pipeline/filters/allocation_failure_test.cxx).
For the isolated executable:

```sh
unset LD_PRELOAD ASAN_OPTIONS LSAN_OPTIONS
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
export LD_LIBRARY_PATH="$PWD/build/oom-validation:$PWD/build/oom-validation/base-plain/lib:$(clang-22 --print-runtime-dir):/usr/lib/llvm/22/lib64"
timeout 90 build/oom-validation/persistent-tests test_track_configuration_allocation_failure
```

The shared incremental build uses `cmake --build` with `-j"$(nproc)"`.
Its `/fast` targets avoid the previously archived, unrelated BuildMaster
generated-pkg-config reinstall collision. Base prerequisite revisions and local
package hashes are recorded in [local-barriers](../local-barriers/README.md).

## Remaining reconciliation, not a warning-free claim

The archived 238 local noexcept-barrier emissions are a historical inventory,
not a new count of defects after this batch. All 251 barrier/bounded-formatting
emissions now have [exact-site treatments](../../../static-analysis/final/oom-batch/README.md).
clang-tidy exit 0 alone is not proof of warning-free or universal exception safety.

Residual warnings include bounded formatting/index paths as well as explicitly
retained semantic oversized-string risks for caller-supplied filter/model names.
Do not label all warnings false positives or claim universal OOM safety.
Allocation injection cannot prove all possible runtime failures.

The original failed-case disposition remains 39 canonical passing follow-ups
and eight user-authorized proprietary NVIDIA exceptions, not 47 sanitizer passes.
Only the documented NVIDIA-driver suppression is used in shared validation.

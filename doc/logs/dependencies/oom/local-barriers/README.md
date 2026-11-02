# Final constructor, report and subtitle validation

## Revisions and scope

- Multimedia `2939592f`: nothrow subtitle shell, checked text allocation and codec guards.
- Multimedia `bb19c73d`: allocating constructor/attachment propagation, partial-state cleanup and startup/tap failure handling.
- Multimedia `605d7742`: report snapshot/virtual override allocation contracts.
- Base `8369f80`: lazy allocation-free telemetry construction.
- Base `696de23`: allocating Map/Set constructor propagation, source-preserving failed move and allocation-free move assignment.
- Base `ce1baac`: previously committed observational telemetry unwind guard.

These are local commits. No release, tag, published package, version or SONAME was changed.
Only the private validation prefix and local cached Base ZIP were refreshed.

## Results

| Validation | Result | Exit code |
|---|---|---:|
| Final shared host/support/affected test executable build | PASS | 0 |
| Isolated UBSan persistent exhaustion, including three new regressions | 9 PASS | 0 each |
| Shared strict ASan/UBSan/LSan affected behavior selection | 7 PASS / 3 intentional SKIP | 0 |
| Base normal Telemetry, allocator injection, Map and Set selection | 4 PASS | 0 |
| Base ASan same selection | 3 PASS / allocator injection SKIP | 0 |

The three ASan skips are `test_stage_factory_allocation_failure`,
`test_filter_report_allocation_failure` and `test_subtitle_allocation_failure`:
the glibc allocator interposer cannot coexist with ASan's allocator. All three
actually execute and pass in the isolated UBSan run. Skips are not counted as passes.
The factory sweep rejects 80 Step and 96 CountFrames allocation positions.
The report regression exercises both CountFrames and VMAF. Subtitle coverage
includes empty shell, moved-from shell, three FFmpeg allocation positions,
partial cleanup and successful reuse.

## Commands and provenance

Shared build, after installing the committed Base into the private prefix:

```sh
cmake --build /tmp/BMDATA/sanitizer-clean-final -j"$(nproc)" --target \
  StormByte-Multimedia/fast multimedia_pipeline_test_support/fast \
  MultimediaPipelineAllocationTests/fast \
  MultimediaPipelineCoordinatorAllocationTests/fast \
  MultimediaSimplePlayerTests/fast MultimediaPipelineFilterTests/fast
```

The `/fast` path avoids an already recorded unrelated BuildMaster pkg-config
reinstall collision. Sources are recompiled when their installed Base headers
change; this is not a claim of a second full clean-provider rebuild.
Configure/build and Base install logs are preserved here or under
[builds](../../../builds/).

For shared tests, unset `LD_PRELOAD`, set the clang-22 runtime and libc++ runtime
directories in `LD_LIBRARY_PATH`, and use:

```sh
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
export LSAN_OPTIONS="suppressions=$PWD/doc/logs/reproduce/nvidia-driver.lsan:print_suppressions=1"
ctest --test-dir /tmp/BMDATA/sanitizer-clean-final/test \
  --output-on-failure --no-tests=error --timeout 90 \
  -R 'test_(transcoder_plan_allocation_failure|transcoder_empty_plan_owner|stage_factory_allocation_failure|filter_report_allocation_failure|subtitle_allocation_failure|filter_count_frames|filter_psnr|filter_ssim|filter_xpsnr|filter_graph_delayed_output_and_eof)$'
```

The actual CTest root is the `test` subdirectory. A preliminary invocation
against the build root found no tests and was not accepted as validation.
[shared-ctest.log](shared-ctest.log) and [shared-LastTest.log](shared-LastTest.log)
preserve the successful final selection.

For isolated tests, use the built `build/oom-validation/persistent-tests`,
`UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`, unset `LD_PRELOAD`, and set:

```sh
export LD_LIBRARY_PATH="$PWD/build/oom-validation:$PWD/build/oom-validation/base-plain/lib:$(clang-22 --print-runtime-dir):/usr/lib/llvm/22/lib64"
timeout 45 build/oom-validation/persistent-tests test_stage_factory_allocation_failure
timeout 45 build/oom-validation/persistent-tests test_filter_report_allocation_failure
timeout 45 build/oom-validation/persistent-tests test_subtitle_allocation_failure
```

The other six exact case names and their exit codes are in the individual logs.
The runnable main/interposer and assertions are checked into
[allocation_failure_test.cxx](../../../../../test/pipeline/filters/allocation_failure_test.cxx).
Base prerequisite changes and their regressions are preserved as git-am patches
alongside the original Base follow-up test logs.

SHA-256:

- Final Base ASan library and private shared-prefix library:
  `6aa1beabc5182658bd489f26451ba1a059cbbbe84bc88b021e879c7c08c399d1`.
- Base plain library selected for isolated tests:
  `aba4f6259dce62a3dced2d72dc4254151c030f86c6637d4c25467d2982cce396`.
- Refreshed local cached `StormByte-2.2.3-linux-x64-libc++.zip`:
  `a073eb8e716fbb7fc3bb7839612168288001ac71b68e88a129960874d96878fe`.

## Explicit limits

This section describes this earlier twelve-site batch. The additional 251
barrier/bounded-formatting emissions are now treated by the
[final batch](../final-batch/README.md); the residual count below is historical.

This closes the twelve specifically mapped pending subtitle/constructor/report
emissions, not every allocation site in Multimedia. The archived static
reconciliation still contains 238 source-confirmed local noexcept-barrier
emissions, grouped shared causes rather than 238 independently reproduced bugs.
The focused clang-tidy rerun of seven changed translation units compiled
successfully but still emits warnings; exit 0 does not mean warning-free.
See [static reconciliation](../../../static-analysis/final/README.md).

No whole-suite rerun was performed. The original 47 failing cases retain their
separate disposition: 39 canonical passes and eight named proprietary NVIDIA
exceptions, not 47 sanitizer passes. These results do not authorize releases
or tag/retag operations, and they do not prove universal OOM safety.

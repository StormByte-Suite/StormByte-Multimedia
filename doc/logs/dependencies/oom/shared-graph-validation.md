# Patched Base and shared sanitizer graph validation

## Revisions and package

- Base fix: `ce1baac254e8cb20055398c1302d28930b445229`.
- Multimedia coordinator: `83e31c4e2d59a24008f3f688909481b1585cb530`.
- Real dependency-teardown regressions: `23b207366d227a9a45c61f2498125d3012660f56`.
- Base remains version 2.2.3; no release, tag or push was performed.
- Dedicated Base build: `build/oom-validation/base-asan`, PGO off, LTO off.
- Patched `libStormByte.so.2.2.3` SHA-256:
  `15a4c94982b841e75ded688981e291f596ccb8d4e7cec946b12274f5340de396`.
- Local ZIP `StormByte-2.2.3-linux-x64-libc++.zip` SHA-256:
  `1abd1b803afdaf0f48fd9653ce9709aa875572d6341eea87004a7384049fcfe8`.

The patched Base was installed into `/tmp/BMDATA/base-oom-package`, repackaged
at the existing download-cache filename and copied into the existing sanitizer
graph's private install prefix. No system prefix was changed. The previous ZIP
is retained in session storage. This refresh is a local audit package, not a
published release artifact.

## Incremental build

Configure succeeded. The normal multi-target build subsequently failed in an
unchanged BuildMaster bzip2 install rule because its generated `bzip2.pc`
already existed. This is not a source/compiler failure and is not reported as
a passing build:

- [Configure](../../builds/oom-coordinator-configure.log), exit 0.
- [Normal build](../../builds/oom-coordinator-build.log), exit 2.

With the patched Base already installed and all provider objects available,
the affected host and tests were rebuilt through CMake's Makefile `/fast`
targets, avoiding the unrelated repeated provider install rule:

```sh
cmake --build /tmp/BMDATA/sanitizer-clean-final -j"$(nproc)" \
  --target StormByte-Multimedia/fast multimedia_pipeline_test_support/fast \
  MultimediaPipelineCoordinatorAllocationTests/fast \
  MultimediaPipelineAllocationTests/fast MultimediaSimplePlayerTests/fast
```

[Incremental build output](../../builds/oom-coordinator-fast-build.log),
exit **0**. This is an incremental validation, not a new from-scratch build.

## Tests

[Shared strict sanitizer result](../../runs/followup/oom-final.xml):
**4 passed, 6 skipped**, exit **0**, 40.74 seconds.

The passing cases are plan-factory allocation exceptions, empty plan ownership,
the canonical live-audio regression and ordinary repeated-video remux.
The six persistent libc allocator-interposition cases intentionally return
77 under ASan: overriding the allocator conflicts with ASan's interceptors.
They are not counted as passing injection coverage.

Persistent exhaustion was exercised separately with the isolated UBSan build,
real patched Base and real Buffer teardown: **9 passed**, no retained
reader/writer owners. See
[real-teardown results](multimedia-real-teardown-results.txt),
[initial isolated results](multimedia-isolated-ubsan-results.txt), and
[Base focused validation](base-fix-validation.txt).

The original unpatched dependency failure backtraces remain preserved.
The Base fix skips observational telemetry sampling during exception unwinding;
the regression separately checks ordinary destruction under exhausted
allocation with no active exception. No blanket allocation or exception
suppression was added to Multimedia.

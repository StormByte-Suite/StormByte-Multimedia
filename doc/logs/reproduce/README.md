# Reproduction guide

The authoritative patch bases and order are in [PR-DRAFTS.md](../../PR-DRAFTS.md)
and [patches.tsv](patches.tsv). Run against the matching patched sources.
Do not apply Multimedia patch paths from the Multimedia root to an upstream
repository: `git am` must run inside that upstream repository.

## Isolated programs

These are optional reference checks, not registered CTest requirements.
Use a Clang toolchain with ASan/UBSan runtimes and no `NDEBUG`, because the
programs use assertions. Each command builds a fresh executable, preserves its
compiler/runtime output and records the real exit status.

```sh
bash doc/logs/reproduce/run-standalone.sh openh264 /path/to/patched/openh264 /tmp/check-openh264
bash doc/logs/reproduce/run-standalone.sh fdk-aac /path/to/patched/fdk-aac /tmp/check-fdk
bash doc/logs/reproduce/run-standalone.sh kvazaar /path/to/patched/kvazaar /tmp/check-kvazaar
```

The final argument is a new or existing output directory. Set `CXX` to a
different Clang executable if necessary; the default is `clang++-22`.
Kvazaar's check requires an x86 CPU with SSE4.1.

| Program | Intent | Scope |
|---|---|---|
| [OpenH264 bitwriter](../providers/openh264/standalone/openh264-bitwriter-check.cxx) | 1,000 bitstreams vs a bit-by-bit reference, empty/repeated flush, lengths 1-32 | Patched bitwriter headers only |
| [FDK multiply](../providers/fdk-aac/standalone/fdk-fixed-multiply-check.cxx) | 100,008 trials, extrema, mixed widths vs widened arithmetic | Generic fixed-point multiply helpers |
| [FDK scale](../providers/fdk-aac/standalone/fdk-scale-check.cxx) | 10,011 trials, shifts -31..31, normal/alternate saturation widths 8/16/24/32 | Scalar/in-place/saturating scaling; also reproduces original saturation error |
| [Kvazaar SAD](../providers/kvazaar/standalone/kvazaar-sad-check.cxx) | 15,872 regular/vertical SAD comparisons, offsets 0-15, heights 1-31 | Widths 4/8 and residual-row handling |

Captured compiler and run output is under each provider's `standalone/logs/`.
These checks do not certify every architecture-specific implementation or every
codec path. On an unpatched source tree, some checks fail earlier than the bug
of interest; use predecessor patches to isolate an individual fix.

## Existing Multimedia reproducers

[ctest-reproducers.tsv](ctest-reproducers.tsv) maps the provider-focused cases
to existing source functions/dispatch entries, CTest executable commands and
the recorded test result. The test tree supplies its own `main` and fixtures.
[fixtures.tsv](fixtures.tsv) records tracked fixture hashes without duplicating
the media. Use the same repository checkout and
[fixture provenance](../../../test/files/README.md).

With the sanitized build already configured:

```sh
export LD_LIBRARY_PATH="$(clang-22 --print-runtime-dir):/usr/lib/llvm/22/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1

# Substitute the exact test name from ctest-reproducers.tsv.
ctest --test-dir /tmp/BMDATA/sanitizer-clean-final/test -j4 \
  --output-on-failure -R '^pipeline\.test_transcoder_kvazaar_pads_unaligned_video$'

# The fourteen-test provider selection:
bash doc/logs/reproduce/run-provider-tests.sh /tmp/BMDATA/sanitizer-clean-final /tmp/provider-check

# Complete existing suite; this is expected to FAIL at this snapshot.
ctest --test-dir /tmp/BMDATA/sanitizer-clean-final/test -j4 \
  --output-on-failure --output-junit /tmp/full-strict.xml
```

The CTest root is the build's `test` directory, not the build root. The current
complete suite is 377 PASS / 47 FAIL / 11 SKIP, exit 8. `detect_leaks=0` is
**not** an equivalent validation command.

## Clean configure/build recipe

The original audit used Linux, Clang 22, libc++, shared ASan/UBSan, bundled
providers and all GPL/nonfree codec options enabled. Instrument Base, Logger,
System and Buffer consistently and prepare the local StormByte ZIPs first.
Package identities/hashes available on the original machine are recorded in
[snapshot.json](../snapshot.json); absence of old package content is not proof
that a published ZIP is equivalent.

Use one Multimedia configure/build graph at a time: bundled dependency graphs
share source checkouts and BuildMaster may reset them while applying patches.
Use a previously nonexistent build path; do not delete arbitrary paths to
simulate a clean build.

```sh
export BUILDMASTER_DATADIR=/tmp/BMDATA
export LD_LIBRARY_PATH="$(clang-22 --print-runtime-dir):/usr/lib/llvm/22/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Preload is for build-time executables only, not the final CTest command.
(
  export LD_PRELOAD="$(clang-22 --print-runtime-dir)/libclang_rt.asan-x86_64.so"
  export ASAN_OPTIONS=detect_leaks=0
  cmake -S . -B /tmp/BMDATA/sanitizer-new-evidence \
    -DCMAKE_C_COMPILER=clang-22 -DCMAKE_CXX_COMPILER=clang++-22 \
    -DCMAKE_C_FLAGS= -DCMAKE_CXX_FLAGS=-stdlib=libc++ \
    -DCMAKE_SHARED_LINKER_FLAGS=-shared-libasan \
    -DCMAKE_EXE_LINKER_FLAGS=-shared-libasan \
    -DCMAKE_CXX_STANDARD_LIBRARIES=-stdlib=libc++ \
    -DENABLE_ASAN=ON -DENABLE_PGO=OFF -DENABLE_LTO=OFF \
    -DENABLE_TEST=ON -DBUILD_SHARED_LIBS=ON \
    -DWITH_STORMBYTE=BUNDLED -DWITH_GPL=ON -DWITH_NONFREE=ON &&
  cmake --build /tmp/BMDATA/sanitizer-new-evidence -j"$(nproc)"
)
```

Reset the four base flag cache values shown above on reconfiguration because
the sanitizer option appends flags. GNU/Linux processor-count parallelism is
intentional; do not replace it with a hard-coded smaller job count.

## Interpretation of historical diagnostics

- A different later sanitizer failure is not recurrence of an already fixed bug.
- A warning in clang-tidy output is not automatically a confirmed bug.
- Twelve failed test processes in the latest full run expose NVIDIA `cuInit`
  allocation leaks. The distinct Vorbis residue allocation needs its own
  ownership investigation.
- Earlier `historical/` runs used recover/suppression experiments. Their options
  do not apply to the current strict ASan/UBSan/LSan results.
- Preview deadlines, hardware preflight failures, timeouts and unfinished OOM
  boundaries are not waived by a successful provider-only check.

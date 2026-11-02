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
bash doc/logs/providers/vorbis/standalone/run-residue-check.sh "$PWD" /tmp/BMDATA/sanitizer-clean-final /tmp/check-vorbis
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
| [Vorbis residue ownership](../providers/vorbis/standalone/vorbis-residue-leak-check.cxx) | Minimal six-channel setup/clear and 144 configurations: six rates, eight channel counts, three qualities | Links the rebuilt instrumented Vorbis/Ogg static libraries |

Captured compiler and run output is under each provider's `standalone/logs/`.
These checks do not certify every architecture-specific implementation or every
codec path. On an unpatched source tree, some checks fail earlier than the bug
of interest; use predecessor patches to isolate an individual fix.

## Existing Multimedia reproducers

### Latest narrowed follow-up

Do not repeat the full suite to reproduce the remaining issues.
[Failed-path selection](../runs/followup/failed-paths-selection.txt) lists the
fifteen provider cases that now pass across the RPU, restoration and
joint-convolution follow-ups. The earlier failed-path run preserves both real
Dolby Vision leaks. The RPU follow-up uses no suppressions; subsequent SVT
runs use only `nvidia-driver.lsan`, with leak detection still enabled.

The [remaining-path selection](../runs/followup/remaining-selection.txt) was
derived from the original full-suite failures minus passing follow-ups.
Its Vorbis surround entry was already validated separately in the provider
standalone folder and was explicitly excluded from the nineteen-test command.
That run has three passes and sixteen failures; the seven preview failures
also reproduce serially. Full `LastTest.log` copies are preserved alongside
each XML because CTest truncates verbose per-test output in JUnit.

For the x265 helper reference main:

```sh
clang++-22 -std=c++17 -DX265_DEPTH=8 -DX265_NS=x265 -DHIGH_BIT_DEPTH=0 \
  -fsanitize=address,undefined -shared-libasan -fno-omit-frame-pointer \
  -I/path/to/patched/x265/source/common -I/path/to/patched/x265/source \
  -I/tmp/BMDATA/sanitizer-clean-final/stormbyte-buildmaster/bm/x265 \
  doc/logs/providers/x265/standalone/x265-coefficient-context-check.cxx \
  -o /tmp/x265-context-check
LD_LIBRARY_PATH="$(clang-22 --print-runtime-dir):/usr/lib/llvm/22/lib64" \
  ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 /tmp/x265-context-check
```

The main compares 4,368,000 real helper results against a reference, including
the final coefficient-group boundary. It is not a new CTest dependency.

Repeat one narrowed group, preserving the command, exit code, XML and complete
CTest log (choose a new output directory):

```sh
bash doc/logs/reproduce/run-followup-tests.sh \
  /tmp/BMDATA/sanitizer-clean-final svt /tmp/svt-followup
```

Other groups are `providers`, `remaining`, `preview` and `deadlines`.
`providers` deliberately runs without suppressions; the others use only the
approved NVIDIA allocation exception.
The `nvenc` group additionally permits CUDA's address-space mapping with
`protect_shadow_gap=0`; its raw failing LSan result is preserved as documented
in the [driver exception](nvidia-driver-exception.md).

### NVIDIA sanitizer initialization constraint

**Update:** `protect_shadow_gap=0` permits CUDA initialization and context
creation while preserving ASan instrumented checks, UBSan and LSan. NVENC
then exposes a separate driver-only session leak, reproduced without FFmpeg
or StormByte. See the [named driver exception](nvidia-driver-exception.md);
the eight affected CTest exits remain failing and are not rewritten as passes.

The [isolated CUDA driver main](cuda-init-check.c) loads only `libcuda.so.1`;
it does not link FFmpeg or StormByte. On this host the same program succeeds
without sanitizers and returns `CUDA_ERROR_OUT_OF_MEMORY` from `cuInit` with
ASan/UBSan. The CUDA return code is a real availability failure, not an LSan
report, so the leak suppression cannot make NVENC available.

```sh
clang-22 -Wall -Wextra -Werror doc/logs/reproduce/cuda-init-check.c \
  -ldl -o /tmp/cuda-init-plain
clang-22 -Wall -Wextra -Werror -g -fsanitize=address,undefined -shared-libasan \
  doc/logs/reproduce/cuda-init-check.c -ldl -o /tmp/cuda-init-sanitized
/tmp/cuda-init-plain
LD_LIBRARY_PATH="$(clang-22 --print-runtime-dir):/usr/lib/llvm/22/lib64" \
  ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  LSAN_OPTIONS="suppressions=$PWD/doc/logs/reproduce/nvidia-driver.lsan:print_suppressions=1" \
  /tmp/cuda-init-sanitized
```

Captured outcomes: [plain: exit 0](../runs/followup/cuda-init-plain.log),
[instrumented: exit 1](../runs/followup/cuda-init-sanitized.log). This isolates
an external sanitizer/driver constraint; the eight failed NVENC tests remain
unvalidated and have not been reclassified as passed or waived.

### Live-audio fixture probes

**Update:** the canonical cycling test now uses the fixed
`test/files/video/live_audio_switch_long.mkv` fixture, generated once and
documented in the [fixture inventory](../../../test/files/README.md).
[Strict direct run without an override: exit 0](../runs/followup/live-audio-canonical-direct.log).
The following probes document how the fixture problem was isolated.

The default two-second 1080p fixture is still a failing case under instrumented
decode/filter overhead. Merely extending its wait does not fix that result.
The [deadline run](../runs/followup/test-deadlines-LastTest.log) records the
switching sequence and the terminal wait. A twelve-second low-resolution
override also fails an epoch assertion; a longer stream-copy repeat passes
all original assertions, including the 500 ms inter-frame limit:

```sh
ffmpeg -v error -nostdin -n -stream_loop 7 \
  -i test/files/video/interleave_long.mkv -map 0 -c copy /tmp/live-audio-96s.mkv
```

For just the cycling case, use the same sanitizer environment documented above:

```sh
STORMBYTE_MM_LIVE_INPUT=/tmp/live-audio-96s.mkv \
  ctest --test-dir /tmp/BMDATA/sanitizer-clean-final/test -j1 \
  -R '^preview.test_live_audio_tracks$' --output-on-failure
```

The generated media is not duplicated here; its source fixture is tracked and
the command above regenerates it without re-encoding.
[Twelve-second probe: exit 8](../runs/followup/live-audio-long-fixture-probe.xml),
[96-second probe: exit 0](../runs/followup/live-audio-extended-fixture-probe.xml).
The alternate-input result does not mark the canonical fixture as passing.

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

```

Do not rerun the complete suite during this follow-up. Select failed names from
[failures.tsv](../runs/current/failures.tsv) and directly related regressions.

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
  allocation leaks. The user authorized excluding only these proprietary driver
  allocations. For affected tests, add
  `LSAN_OPTIONS=suppressions=/absolute/path/to/doc/logs/reproduce/nvidia-driver.lsan:print_suppressions=1`.
  This matches `cuInit` stack frames, not general allocation functions or all of
  CUDA/FFmpeg. Keep `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1` and retain the
  suppression statistics. It does not waive NVENC capability/encode failures.
- The distinct 2,848-byte Vorbis leak was reproduced without FFmpeg or CUDA and
  fixed by reusing the residue allocation shared by short/long encoding modes.
  Both the minimal case and all 144 setup/clear configurations pass unsuppressed.
- Earlier `historical/` runs used recover/suppression experiments. Their options
  do not apply to the current strict ASan/UBSan/LSan results.
- Preview deadlines, hardware preflight failures, timeouts and unfinished OOM
  boundaries are not waived by a successful provider-only check.

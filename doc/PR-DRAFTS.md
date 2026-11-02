# Upstream patch submissions

This collection contains **64 independent bug-fix commits for ten source
repositories**. Each patch is a `git format-patch` email with the configured
repository author, integrated into Multimedia's bundled build in a separate
commit with a changelog entry.

No new provider-dependent Multimedia CTest tests were added. SYSTEM-provider
builds do not acquire an expectation that external libraries contain these fixes.
The numerical checks mentioned below are isolated validation programs, not new
CTest requirements.

**The audit is not complete. Releases and tag/retag operations remain on hold.**
No upstream PR has been opened automatically.

The complete available evidence is now retained in the repository under
[doc/logs](logs/README.md), organized by build, test run and provider rather
than depending on temporary build/session directories. It includes
[full-suite results](logs/runs/current/test-results.tsv),
[remaining failures](logs/runs/current/failures.tsv),
[all patch mappings](logs/reproduce/patches.tsv),
[existing triggering tests](logs/reproduce/ctest-reproducers.tsv),
[standalone mains and runnable commands](logs/reproduce/README.md),
original diagnostics, fixture hashes and integrity checks.
This preservation work does not close the audit: subsequent validation should
target the failed cases, including leaks, without rerunning the whole suite.

## Application and base revisions

Apply patches from the respective source repository root, not from Multimedia.
For LAME, use the nested source repository, not its CMake wrapper.

```sh
git am /absolute/path/to/the-first.patch /absolute/path/to/the-next.patch
```

Apply each repository's fixes in ascending filename order. Numerical prefixes
belong to Multimedia's patch lists: unrelated compatibility patches with lower
numbers are not prerequisites for this collection. The FDK-AAC 0010 and 0011
changes touch different files and have also been checked in authoring order.

| Repository | Tested base revision | Fixes |
|---|---|---:|
| FFmpeg source snapshot bundled by Multimedia | `5a215d7ce4de21ec5c34214994eea0ac13dc9054` | 4 |
| SVT-AV1 | `9292ec8e32bce26f781f277ec8739b53426c4300` | 10 |
| Kvazaar | `6040962bed5cc68c5ad01234c38c08b8b2822068` | 10 |
| x265 | `e444744c03978c1fb4e037168967020cf2648427` | 8 |
| Vorbis | `0657aee69dec8508a0011f47f3b69d7538e9d262` | 5 |
| VMAF | `74f930d17de8c58e7c2ce55e2b897b0030ebd8ef` | 2 |
| FDK-AAC | `716f4394641d53f0d79c9ddac3fa93b03a49f278` | 20 |
| x264 | `00852f599572d10798b2dafd2c5286213d0ce795` | 1 |
| LAME | `1f5cc9487284d5950343aa5d4f70de433468070a` | 2 |
| OpenH264 | `652bdb7719f30b52b08e506645a7322ff1b2cc6f` | 2 |

All 48 patches were applied with `git am` in ascending filename order on clean
copies of all ten documented bases. Each resulting Git tree exactly matched
its authored tree. Follow-up batches were also checked on clean predecessor
trees. Application against current upstream HEAD has **not** been checked.
Rebase and revalidate before submission if it differs from the base above.
The FFmpeg snapshot includes a Meson build port; these changes touch codec
sources, not the build port.

## Validation summary

- Latest failed-path follow-up: all **15 selected provider paths now pass**
  across focused runs, not a rerun of the full suite. Eleven passed without
  suppressions in [the RPU follow-up](logs/runs/followup/rpu-followup.xml);
  SVT repeated cancellation passed in
  [the restoration follow-up](logs/runs/followup/svt-tail-followup.xml), and
  all three interleave cases passed in
  [the joint-convolution follow-up](logs/runs/followup/svt-joint-followup.xml),
  exit **0** for that final three-test run. The latter runs retain leak
  detection and use only the approved `leak:cuInit` exception.
- Both per-frame Dolby Vision leaks are fixed: FFmpeg's generated RPU temporary
  and x265's owned frame copy. All seven x265 Dolby Vision paths pass with
  **no leak suppressions**. Before/after full output is retained because JUnit
  truncates lengthy diagnostic streams:
  [before](logs/runs/followup/failed-paths-LastTest.log),
  [after](logs/runs/followup/rpu-followup-LastTest.log).
- Fifteen additional mail patches were applied on clean predecessor trees;
  all five resulting provider trees exactly match their authoring trees.
  [Verification](logs/reproduce/git-am-followup-verification.log).
- The other **19 unresolved original cases** were run separately:
  **3 passed / 16 failed**, exit **8**. HDR10+ roundtrip and both watermark
  paths now pass. Remaining failures are seven preview tests, a duration-hint
  timeout and eight NVENC preflight/encode tests. All eight NVENC failures
  explicitly report `cuInit` returning `CUDA_ERROR_OUT_OF_MEMORY`, despite
  the GPU reporting free memory; these are **not waived** by the leak exception.
  [Results](logs/runs/followup/remaining-paths.xml).
- All seven preview failures also reproduce serially without concurrent test
  load: **0 passed / 7 failed**, exit **8**.
  [Serial evidence](logs/runs/followup/preview-serial.xml).
- Overall the original 47 failing cases have **31 passing follow-up results
  and 16 unresolved failures**. This is a union of focused runs, not a new
  full-suite result or a release gate. OOM/static-analysis follow-up is also
  still pending; releases and tags remain held.

#### Subsequent preview and duration investigation

**Latest update:** the canonical live-audio cycling regression now uses a fixed
96-second synthetic H.264/AC-3 fixture, committed with its generation command
and SHA-256. It passes with no input override under strict ASan/UBSan/LSan;
the six track changes and original epoch/timestamp/500 ms assertions remain.
[Canonical direct-run evidence](logs/runs/followup/live-audio-canonical-direct.log).

CUDA/NVENC is now reachable under ASan using `protect_shadow_gap=0`, which
permits the driver's virtual-address mappings without disabling instrumented
memory checks or leak detection. The eight NVENC paths complete as **six
functional passes and two unsupported H.264 10-bit skips**, then LSan reports
96/192-byte allocations. A new driver-only NVENC session main reproduces
exactly the same 96-byte/two-allocation leak after successful encoder/context
destruction, without linking FFmpeg or StormByte. These are covered by the
user-authorized proprietary NVIDIA leak exception, not suppressed through
broad libc allocation rules and not reported as clean sanitizer passes.
[Named exception, all affected tests, commands and evidence](logs/reproduce/nvidia-driver-exception.md).
The prior plain build provides an independent **6 passed / 2 skipped** control,
exit **0**; the instrumented run remains exit **8** due to those driver leaks.

The original 47 failing cases therefore now comprise **39 canonical passing
follow-ups and 8 explicitly classified external-driver exceptions**, with
functional checks passing or skipping appropriately for the available GPU.
This closes that failed-test list under the authorized driver exception,
not the separate pending OOM/static-analysis work or the release gate.

Earlier iterations, retained for provenance:

The deadline follow-up resolves six more original cases without changing their
functional assertions: HDR SDR/HDR preview, live audio selection, subtitle
decode, faulted reader ownership and duration-hint remux. The latter passes
unchanged in **26.29 seconds**, beyond CTest's previous 20-second cutoff.
An isolated HDR probe likewise passes in **22.48 seconds**, beyond the old
five-second in-process wait. Preview waits now honor the existing instrumented
test deadline; CTest allows 480 seconds for ASan/PGO, as pipeline tests already do.
[Eight-case results: 6 passed / 2 failed, exit 8](logs/runs/followup/test-deadlines.xml).

The seek/epoch regression had a separate asynchronous test race: clock progress
did not imply that three video frames had already reached the display. It now
waits for both conditions before pausing and retains all subsequent assertions.
[Focused result: 1 passed, exit 0](logs/runs/followup/preview-frame-readiness.xml).

The original live-audio-track cycling case **still fails**: its two-second
1080p fixture is exhausted while six track changes are being exercised under
instrumentation. Its diagnostic output reaches position 5.666 before the next
audio-track enable, with no remaining audio to deliver. Reusing the existing
12-second low-resolution fixture still fails an epoch assertion; a 96-second
stream-copy repetition passes all original timing/epoch assertions in 33.85
seconds. This is diagnostic evidence, **not a passing result for the default
fixture**. [Reproduction](logs/reproduce/README.md#live-audio-fixture-probes).

Latest consolidated state: **38 of the original 47 failures have passing
follow-up results; 9 remain unresolved** (the default live-audio-track test and
eight NVENC tests). The full suite was not rerun. See
[per-original-failure state](logs/runs/followup/latest-original-failures.tsv).

The NVENC constraint is reproduced in a small standalone CUDA driver program:
plain build **exit 0**, instrumented build **exit 1**, reporting
`CUDA_ERROR_OUT_OF_MEMORY` from `cuInit`. No FFmpeg or StormByte code is linked.
This confirms an external sanitizer/driver initialization incompatibility on
this host, not a Multimedia allocation leak. It has **not** been waived.
[Source, commands and output](logs/reproduce/README.md#nvidia-sanitizer-initialization-constraint).

The following complete-suite figures are historical and remain unchanged:

- Linux, Clang 22, ASan + UBSan + LSan; PGO and LTO disabled.
- A new build directory, `/tmp/BMDATA/sanitizer-clean-final`, was created without
  reusing dependency objects, installations, CMake caches or BuildMaster stamps.
  Only the prepared sanitizer-instrumented StormByte ZIP packages were reused.
- That initial clean configure/build completed with exit code **0**, including
  21 upstream fixes. OpenH264's unsigned-mask fix and subsequent batches were
  added afterwards and rebuilt in the same graph. Do not describe all 48 fixes
  as having been compiled together in the initial from-scratch build.
- Every subsequent configure/build, including the latest 48-fix configuration,
  completed with exit code **0** and processor-count-based build parallelism.
- Tests use `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1` and
  `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`, without sanitizer suppression.
- Latest complete provider-focused batch: **13/14 passed, 1 failed**, exit
  code **8**. The failing test is `pipeline.test_fdk_aac_encodes_stereo_mp4`,
  currently stopped by a negative left shift in `libAACenc/src/adj_thr.cpp:527`.
- Full strict suite on the current rebuilt graph: **377 passed, 47 failed,
  11 skipped**, 435 total, exit code **8**, elapsed 680.17 seconds. This is a
  failing validation result, not a release waiver. The NLMeans filter test
  completed successfully in 244.69 seconds.
- The existing native mono Opus preview and
  `pipeline.test_reencode_aom_av1_dovi_only` reproducers passed separately.
- Isolated OpenH264 bitwriter check: **1,000 bitstreams**, compared bit-for-bit
  against a simple reference, including empty/repeated flush and 1-32-bit writes.
- Isolated FDK multiply check: **100,008 trials**, including extrema and all
  generic multiply variants, compared with widened integer references.
- Isolated FDK scaling check: **10,011 trials**, signed shifts -31 through 31,
  and normal/alternate saturation with 8-, 16-, 24- and 32-bit destinations.
  This check also discovered the separately fixed negative-saturation boundary.
- Isolated Kvazaar SAD check: **15,872 cases**, both input offsets 0-15,
  widths 4/8 and heights 1-31, compared with scalar regular/vertical SAD.
- All four isolated checks passed with strict ASan/UBSan, exit code **0**.
- Follow-up Vorbis allocation fix: minimal six-channel setup/clear and **144**
  rate/channel/quality configurations pass without leak suppressions, exit **0**.
  The previously failing surround encode test also passes, exit **0**.
  [Before/after evidence and runnable main](logs/providers/vorbis/standalone/).

### Additional PR material for the fifteen follow-up fixes

Append these points to the corresponding provider submissions below. Patch
paths, subjects, upstream commit IDs and resulting tree hashes are recorded in
[patches.tsv](logs/reproduce/patches.tsv).

| Provider | Additional patches | Confirmed defect and validation |
|---|---|---|
| FFmpeg | `0012` | Generated Dolby Vision RPU temporary leaked after `encoder_encode`; release it after synchronous consumption, without changing reordered opaque ownership. Seven Dolby Vision tests pass under unsuppressed LSan. |
| x265 | `0008`-`0010` | Final coefficient-group shift by 64; diamond direction signed overflow; RPU copies leaked on frame reuse/destruction. Real helper reference check covers 4,368,000 comparisons; seven Dolby Vision paths pass under strict sanitizers. |
| Vorbis | `0007` | Decoder first-table search hints shifted a signed value into its sign bit. Use the existing unsigned word type before shifting. Decode roundtrip passes. |
| FDK-AAC | `0017`-`0020` | Negative shifts in minimum-SNR, intensity spectrum/correlation and threshold-log normalization. Preserve fixed-point bit patterns with unsigned shifts and restore signed values before arithmetic use. Stereo and surround encode paths pass. |
| SVT-AV1 | `0007`-`0012` | Misaligned scalar loads in convolution, packing, SAD, SSE4.1 short rows and restoration tails; second-row stores also overflowed signed halfword assembly. Reuse existing unaligned-safe SIMD helpers; load a single restoration lane at its actual 16-bit width. Dolby Vision watermark, repeated cancellation and three interleave paths pass. |

Build and complete per-test diagnostic logs for each iteration are in
[follow-up builds](logs/builds/) and [focused test runs](logs/runs/followup/).
Do not submit proprietary NVIDIA allocations as defects in these providers.

### Provider-focused results

| Provider | Existing test paths | Result |
|---|---|---|
| VMAF | Picture API contract; identical-source remux score | 2 passed |
| OpenH264 | H.264 Matroska; 10-bit input after format conversion | 2 passed |
| x264 | Software decode to H.264 Matroska | 1 passed |
| LAME | MP3 tags through Opus; MP3 cover art | 2 passed |
| Kvazaar | Unaligned 704x396 input, padded HEVC output | 1 passed |
| x265 | HEVC output properties | 1 passed |
| SVT-AV1 | Dolby Vision-only AV1; HDR10 metadata preservation | 2 passed |
| Vorbis | Opus tags preserved through Vorbis encoding | 1 passed |
| FDK-AAC | Surround AAC decoding to FLAC | 1 passed |
| FDK-AAC | Stereo AAC encoding to MP4 | 1 failed: next threshold-adjustment shift |

**This is not a clean-suite or release-readiness claim.** Passing tests validate
their named paths only. FDK encoding gets beyond the fixed reports but has not
completed under strict sanitizers. Preview/timing failures, remaining Multimedia
Transcoder OOM contracts and outstanding clang-tidy triage still need closure.
The proprietary NVIDIA driver leak is separate from these upstream fixes.

### Additional reports from the full-suite run

The broader suite reaches paths outside the fourteen provider reproducers.
The following reports are **not fixed by this collection** and require separate
source-level investigation and validation:

| Provider | Reported location | Diagnostic |
|---|---|---|
| FDK-AAC | `libAACenc/src/adj_thr.cpp:527` | Negative shift in minimum-SNR adjustment |
| FDK-AAC | `libAACenc/src/intensity.cpp:403` | Negative shift in intensity-stereo processing |
| Vorbis | `lib/sharedbook.c:425` | Signed overflow while packing codebook values |
| SVT-AV1 | `Source/Lib/ASM_AVX2/convolve_avx2.h:126` | Misaligned SSSE3 two-tap coefficient read |
| SVT-AV1 | `Source/Lib/ASM_AVX2/convolve_avx2.c:378` | Misaligned convolution fragment read |
| SVT-AV1 | `Source/Lib/ASM_AVX2/pic_operators_intrin_avx2.c:300` | Misaligned four-byte pixel fragment read |
| x265 | `source/common/quant.h:128` | 64-bit shift count of 64 in coefficient context calculation |
| x265 | `source/encoder/motion.cpp:1018` | Signed overflow while extracting motion-search direction bits |

This list is not a claim that the rest of the full suite is clean. Its complete
test result, preview assertions, timeouts and external hardware failures must
be considered separately.

Twelve failing test processes also report allocations in the proprietary
NVIDIA driver's `cuInit` path, in multiples of 456 bytes. They are distinct from
the now-fixed Vorbis residue allocation. The original strict logs remain
unchanged. With the user-approved [cuInit-only LSan exception](logs/reproduce/nvidia-driver.lsan),
all twelve affected tests pass, exit **0**, with leak detection still enabled.
See [selection](logs/runs/followup/nvidia-driver-selection.txt) and
[JUnit evidence](logs/runs/followup/nvidia-driver.xml).
Hardware preflight/encode failures are not covered by this exception.

## Suggested FFmpeg cover message

**Title:** Fix three codec integration issues found with ASan and UBSan

This series fixes three independent issues reproduced with existing
StormByte-Multimedia tests against sanitizer-instrumented FFmpeg:

1. Mono hybrid Opus decoding forms a pointer relative to an absent second CELT
   buffer. Initialize delay pointers only for active channels.
2. Dolby Vision T.35 payloads from external decoders may lack trailing bitreader
   padding. Use separately owned zero-padded storage before EMDF/RPU parsing.
   Keep parser extraction scratch separate; check allocation size and failure.
3. Translate FFmpeg's constrained-baseline H.264 profile to OpenH264's
   `PRO_BASELINE`, rather than treating FFmpeg constraint flags as an enum value.

Packet formats and intended profile policy are unchanged. Mono Opus and the
libdav1d Dolby Vision reproducer pass with strict sanitizers. Both OpenH264
encoder tests now also pass when the independent OpenH264 fixes below are
applied; do not attribute those library fixes to this FFmpeg series.

Patches:
- [0009: Opus delay pointers](../thirdparty/ffmpeg/0009-opusdec-initialize-active-channel-delay-pointers.patch)
- [0010: Dolby Vision input padding](../thirdparty/ffmpeg/0010-itut35-pad-dolby-vision-payloads.patch)
- [0011: OpenH264 profile translation](../thirdparty/ffmpeg/0011-libopenh264enc-map-constrained-baseline.patch)

## Suggested SVT-AV1 PR

**Title:** Remove scalar alignment assumptions from AVX2 and SSE2 fragments

Byte-addressed neighborhoods, two-tap coefficient pairs, deblocking tails and
CDEF directional offsets do not guarantee scalar alignment. Reuse existing
unaligned-safe helpers for each exact-width access. Preserve row order, lane
order and arithmetic; do not disable alignment checking.

The bundled sanitizer build succeeds. Existing HDR10-preservation and
Dolby Vision-only AV1 encoding tests both complete with strict UBSan and LSan.

Patches:
- [0003: Level and half-precision coefficient loads](../thirdparty/ffmpeg/plugins/codecs/video/svt-av1/0003-avx2-unaligned-level-and-coefficient-loads.patch)
- [0004: Full-precision coefficient loads](../thirdparty/ffmpeg/plugins/codecs/video/svt-av1/0004-avx2-unaligned-full-two-tap-coefficients.patch)
- [0005: Four-byte SSE2 deblocking stores](../thirdparty/ffmpeg/plugins/codecs/video/svt-av1/0005-sse2-unaligned-four-byte-deblock-stores.patch)
- [0006: Four-pixel CDEF row accesses](../thirdparty/ffmpeg/plugins/codecs/video/svt-av1/0006-avx2-unaligned-cdef-row-accesses.patch)

## Suggested Kvazaar PR

**Title:** Fix unaligned pixel accesses, strategy signatures and chroma overreads

This series removes scalar alignment assumptions from exact-width prediction,
reconstruction, intra-reference, interpolation and SAD fragments. It preserves
SIMD lane order, rounding and backwards traversal for overlapping subpixel row
shifts. Two AVX2 callbacks are adjusted to match their declared strategy types.

The four-tap vertical chroma paths additionally read eight bytes from a
four-byte coefficient row. The final row overreads the global table. Read
exactly four bytes; only the first two coefficient pairs are used.

The existing unaligned-video encoding test now completes with strict ASan,
UBSan and LSan. An isolated SAD check also matches a scalar reference for
15,872 alignment/height cases.

Patches:
- [0009: Eight-pixel residual/prediction loads](../thirdparty/ffmpeg/plugins/codecs/video/kvazaar/0009-avx2-unaligned-eight-pixel-loads.patch)
- [0010: Reconstruction stores](../thirdparty/ffmpeg/plugins/codecs/video/kvazaar/0010-avx2-unaligned-reconstruction-stores.patch)
- [0011: Eight-pixel DC references](../thirdparty/ffmpeg/plugins/codecs/video/kvazaar/0011-avx2-unaligned-dc-reference-loads.patch)
- [0012: Four-pixel intra references](../thirdparty/ffmpeg/plugins/codecs/video/kvazaar/0012-avx2-unaligned-four-pixel-reference-loads.patch)
- [0013: Quantization strategy signature](../thirdparty/ffmpeg/plugins/codecs/video/kvazaar/0013-avx2-match-quant-strategy-signature.patch)
- [0014: SAO edge strategy signature](../thirdparty/ffmpeg/plugins/codecs/video/kvazaar/0014-avx2-match-sao-edge-strategy-signature.patch)
- [0015: Interpolation fragments](../thirdparty/ffmpeg/plugins/codecs/video/kvazaar/0015-avx2-unaligned-interpolation-row-accesses.patch)
- [0016: Subpixel block copies](../thirdparty/ffmpeg/plugins/codecs/video/kvazaar/0016-avx2-unaligned-subpixel-block-copies.patch)
- [0017: SSE4.1 SAD fragments](../thirdparty/ffmpeg/plugins/codecs/video/kvazaar/0017-sse41-unaligned-sad-fragment-loads.patch)
- [0018: Four-byte chroma coefficient reads](../thirdparty/ffmpeg/plugins/codecs/video/kvazaar/0018-avx2-limit-chroma-filter-loads-to-four-bytes.patch)

## Suggested x265 PR

**Title:** Fix NAL initialization, signed scaling and optional rate-control state

Five independent fixes skip the empty copy from the initial null NAL buffer,
guard full-width bitstream shifts, access temporal-filter scratch only inside
its enabled branch, use the existing bounded motion-vector multiplication in
all builds, and skip residual-cost division when amortization is disabled.

The last issue arises when rate control deliberately sets both the amortization
frame count and fraction to zero near the end of a known-length encode.
Both partial and final accounting must leave the current bits unamortized,
not divide zero by zero and convert NaN to an integer.

The bundled sanitizer build succeeds and the existing HEVC output-properties
test now completes with strict ASan, UBSan and LSan.

Patches:
- [0003: Empty NAL buffer copy](../thirdparty/ffmpeg/plugins/codecs/video/x265/0003-nal-skip-empty-buffer-copy.patch)
- [0004: Full-width bitstream shift](../thirdparty/ffmpeg/plugins/codecs/video/x265/0004-bitstream-avoid-full-width-shift.patch)
- [0005: Optional temporal-filter scratch](../thirdparty/ffmpeg/plugins/codecs/video/x265/0005-lookahead-guard-temporal-estimator-access.patch)
- [0006: Signed motion-vector scaling](../thirdparty/ffmpeg/plugins/codecs/video/x265/0006-mv-scale-signed-vectors-with-multiplication.patch)
- [0007: Disabled amortization division](../thirdparty/ffmpeg/plugins/codecs/video/x265/0007-ratecontrol-skip-disabled-amortization-division.patch)

## Suggested Vorbis PR

**Title:** Fix psychoacoustic bounds/shifts and shared residue allocation ownership

Pack negative Bark-window bounds and bounded negative floor-post deviations
using representable multiplication, not signed left shifts. Clamp both
noise-offset interpolation indices to the final band: even a zero-weight
out-of-bounds operand is still evaluated.

The bundled sanitizer build succeeds. The existing Opus-to-Vorbis metadata
test completes with strict UBSan and leak detection.

Short and long mapping modes may initialize the same residue index. Reuse the
owned residue object rather than overwriting its allocation. A six-channel
encoder setup/clear reproduced a 2,848-byte leak before this fix. The minimal
case, all 144 setup/clear configurations and the formerly failing surround
encode now pass with ASan/UBSan/LSan and no suppression.

Patches:
- [0003: Negative Bark-window bound](../thirdparty/ffmpeg/plugins/codecs/audio/vorbis/0003-psy-avoid-negative-bark-shift.patch)
- [0004: Final noise-offset interpolation band](../thirdparty/ffmpeg/plugins/codecs/audio/vorbis/0004-psy-bound-noise-offset-interpolation.patch)
- [0005: Negative floor-post deviation](../thirdparty/ffmpeg/plugins/codecs/audio/vorbis/0005-floor1-avoid-negative-deviation-shift.patch)
- [0006: Shared residue allocation ownership](../thirdparty/ffmpeg/plugins/codecs/audio/vorbis/0006-vorbisenc-reuse-shared-mode-residue-allocation.patch)

## Suggested VMAF PR

**Title:** Handle small ADM coefficients and signed wavelet packing safely

The AVX2 ADM path computes leading-zero counts and rounding shifts before
blending out small coefficients. Return small values with a zero shift first,
matching the scalar precondition and avoiding `clz(0)` or negative shifts.
Pack negative wavelet coefficients through unsigned bit patterns.

The picture API contract and identical-source remux score tests both pass
with strict ASan, UBSan and LSan.

Patches:
- [0003: Small ADM coefficients](../thirdparty/vmaf/0003-adm-avx2-handle-small-coefficients.patch)
- [0004: Unsigned coefficient packing](../thirdparty/vmaf/0004-adm-avx2-pack-coefficients-with-unsigned-shifts.patch)

## Suggested FDK-AAC PR

**Title:** Remove undefined signed shifts from fixed-point and AAC processing

This series fixes individually reproduced signed conversions and closely
related operations with the same preconditions. Use representable widened
multiplication for bounded conversions, and unsigned bit patterns where modulo
fixed-point scaling is intended. Preserve rounding, precision and the distinct
normal/alternate saturation limits.

A separate boundary fix compares the normal saturation lower bound in the
widened product domain. Previously, `-1` shifted by eight for an 8-bit output
escaped saturation and returned `-256` instead of `-128`.

The bundled sanitizer build succeeds. Surround AAC decoding to FLAC passes.
Isolated multiply and scaling matrices pass against widened references.
**Stereo AAC encoding still fails** at a further negative shift in
`libAACenc/src/adj_thr.cpp:527`. These commits do not claim a sanitizer-clean
encoder. Finish investigating that path before describing the series as such.

Patches:
- [0001: Signed fractional widening](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0001-widen-fractional-samples-without-negative-shift.patch)
- [0002: Logarithm exponent](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0002-log2-avoid-negative-exponent-shift.patch)
- [0003: Power mantissa](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0003-pow-avoid-negative-mantissa-shift.patch)
- [0004: Power-of-two integer exponent](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0004-pow2-avoid-negative-integral-part-shift.patch)
- [0005: Saturating conversion shifts](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0005-scale-avoid-negative-saturating-shifts.patch)
- [0006: Fixed-point products](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0006-fixmul-avoid-negative-product-shifts.patch)
- [0007: Block-switch samples](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0007-block-switch-avoid-negative-sample-shifts.patch)
- [0008: Scalar/in-place scaling](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0008-scale-avoid-negative-value-shifts.patch)
- [0009: Band spectra and energy exponents](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0009-band-energy-avoid-negative-spectrum-shifts.patch)
- [0010: Negative saturation boundary](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0010-scale-check-negative-saturation-in-widened-domain.patch)
- [0011: Psychoacoustic normalization](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0011-psy-avoid-negative-normalization-shifts.patch)
- [0012: TNS spectral normalization](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0012-tns-avoid-negative-spectrum-shifts.patch)
- [0013: LPC coefficient normalization](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0013-lpc-avoid-negative-coefficient-normalization-shifts.patch)
- [0014: LPC analysis/synthesis state](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0014-lpc-avoid-negative-filter-state-shifts.patch)
- [0015: Scalefactor conversions](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0015-scalefactor-avoid-negative-fractional-shifts.patch)
- [0016: MDCT folded samples](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0016-mdct-avoid-negative-folded-sample-shifts.patch)

## Suggested x264 PR

**Title:** Use type-correct worker callbacks instead of function-pointer casts

Use the declared thread-pool and pthread callback signatures, converting object
pointers inside workers. Return null for unused results and preserve encoder
slice results, including direct non-threaded calls.

The software-decode-to-H.264 test completes with strict UBSan and LSan.

- [0002: Worker callback signatures](../thirdparty/ffmpeg/plugins/codecs/video/x264/0002-threadpool-type-correct-worker-callbacks.patch)

## Suggested LAME PR

**Title:** Avoid undefined signed shifts in bit packing and scalefactor gain

Pack reservoir values through unsigned intermediates. Compute negative
subblock scalefactor gain adjustments using bounded multiplication.
Both changes preserve the intended bit representation and gain adjustment.

The MP3 tags-through-Opus and MP3 cover-art tests complete with strict
UBSan and leak checking.

- [0001: Unsigned reservoir packing](../thirdparty/ffmpeg/plugins/codecs/audio/mp3lame/0001-bitstream-use-unsigned-reservoir-shifts.patch)
- [0002: Negative scalefactor gain](../thirdparty/ffmpeg/plugins/codecs/audio/mp3lame/0002-quantize-avoid-negative-scalefactor-shift.patch)

## Suggested OpenH264 PR

**Title:** Fix full-word bitwriter shifts and trailing-bit mask overflow

Do not shift the accumulator by 32 on empty flush or full-word writes into an
empty accumulator. Construct trailing-bit masks with unsigned arithmetic:
the signed `1 << 31` mask path overflows when subtracting one.

Both existing OpenH264 encoder tests pass. An isolated bit-by-bit reference
check validates 1,000 pseudo-random bitstreams, empty/repeated flushes and
write lengths 1-32 with ASan and UBSan.

- [0001: Empty/full-word shifts](../thirdparty/ffmpeg/plugins/codecs/video/openh264/0001-golomb-avoid-empty-word-shifts.patch)
- [0002: Unsigned trailing-bit mask](../thirdparty/ffmpeg/plugins/codecs/video/openh264/0002-golomb-use-unsigned-trailing-mask.patch)

## Preserved evidence and remaining work

Original log names below are retained under categorized folders in
[doc/logs](logs/README.md). Build-directory paths are provenance, not the only
remaining copies. Consult the [inventory](logs/inventory.tsv) for their exact
archived locations:

- `build/sanitizer-validation/clean-final-{configure,build}.log`: initial
  from-scratch build.
- `build/sanitizer-validation/clean-final-focused.{log,xml}`: initial 7/14.
- `build/sanitizer-validation/clean-followup-focused.{log,xml}`: next 10/14.
- `build/sanitizer-validation/clean-{third,fourth,fifth,sixth,seventh,eighth,ninth,tenth}-*`:
  follow-up builds and focused diagnostic progression; some batches deliberately
  run only the remaining failing tests, not all fourteen.
- `build/sanitizer-validation/clean-current-focused.{log,xml}`: latest complete
  provider batch, 13/14 and exit code 8.
- `build/sanitizer-validation/clean-current-full-strict.{log,xml}`: full-suite
  validation on the current rebuilt graph: 377 passed, 47 failed, 11 skipped,
  exit code 8.

The submission archive also includes the four isolated validation sources under
`validation/`: `openh264-bitwriter-check.cxx`, `fdk-fixed-multiply-check.cxx`,
`fdk-scale-check.cxx` and `kvazaar-sad-check.cxx`. They require the corresponding
patched source headers, Clang sanitizer flags, and SSE4.1 for the SAD check;
they are not installed as Multimedia tests.

The same sources now live persistently under the relevant
[provider folders](logs/providers/), with captured compiler/runtime output and
a [standalone runner](logs/reproduce/run-standalone.sh). The
[reproduction guide](logs/reproduce/README.md) records setup, limitations,
dirty Transcoder snapshot and the original dependency-instrumentation boundary.
Raw historical recover-mode logs are explicitly separated from current strict
validation; they are not passing evidence or a waiver.

Remaining release gates include FDK threshold adjustment, full-suite failures,
preview behavior, complete Transcoder OOM handling and remaining static-analysis
triage. The unfinished Transcoder edits are preserved separately and are not
included in any upstream patch integration commit.

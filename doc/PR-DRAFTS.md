# Upstream patch submissions

These are twelve independent bug-fix commits for nine source repositories.
Each patch is a `git format-patch` email with the configured repository author.
No new Multimedia CTest tests were added. Existing SYSTEM-provider builds do
not acquire an expectation that their external libraries contain these fixes.

No PR, push, tag or release has been created. Releases remain on hold.

## Application and base revisions

Apply each repository's patches from that repository's root, not from Multimedia.
The numerical prefixes belong to Multimedia's bundled patch lists; unrelated
compatibility patches with lower numbers are not prerequisites for these fixes.
For LAME, use the nested source repository, not the CMake wrapper directory.

```sh
git am /absolute/path/to/the-first.patch /absolute/path/to/the-next.patch
```

For FFmpeg the order is 0009, 0010, 0011. For LAME the order is 0001, 0002.
All other repositories have one patch in this submission.

| Repository | Tested base revision | Email patches |
|---|---|---|
| FFmpeg source snapshot bundled by Multimedia | `5a215d7ce4de21ec5c34214994eea0ac13dc9054` | [Opus](../thirdparty/ffmpeg/0009-opusdec-initialize-active-channel-delay-pointers.patch), [Dolby Vision](../thirdparty/ffmpeg/0010-itut35-pad-dolby-vision-payloads.patch), [OpenH264 bridge](../thirdparty/ffmpeg/0011-libopenh264enc-map-constrained-baseline.patch) |
| SVT-AV1 | `9292ec8e32bce26f781f277ec8739b53426c4300` | [Unaligned loads](../thirdparty/ffmpeg/plugins/codecs/video/svt-av1/0003-avx2-unaligned-level-and-coefficient-loads.patch) |
| Kvazaar | `6040962bed5cc68c5ad01234c38c08b8b2822068` | [Eight-pixel loads](../thirdparty/ffmpeg/plugins/codecs/video/kvazaar/0009-avx2-unaligned-eight-pixel-loads.patch) |
| x265 | `e444744c03978c1fb4e037168967020cf2648427` | [Empty NAL buffer](../thirdparty/ffmpeg/plugins/codecs/video/x265/0003-nal-skip-empty-buffer-copy.patch) |
| Vorbis | `0657aee69dec8508a0011f47f3b69d7538e9d262` | [Bark bound shift](../thirdparty/ffmpeg/plugins/codecs/audio/vorbis/0003-psy-avoid-negative-bark-shift.patch) |
| VMAF | `74f930d17de8c58e7c2ce55e2b897b0030ebd8ef` | [Small ADM coefficients](../thirdparty/vmaf/0003-adm-avx2-handle-small-coefficients.patch) |
| FDK-AAC | `716f4394641d53f0d79c9ddac3fa93b03a49f278` | [Fractional conversion](../thirdparty/ffmpeg/plugins/codecs/audio/fdk-aac/0001-widen-fractional-samples-without-negative-shift.patch) |
| x264 | `00852f599572d10798b2dafd2c5286213d0ce795` | [Callback signatures](../thirdparty/ffmpeg/plugins/codecs/video/x264/0002-threadpool-type-correct-worker-callbacks.patch) |
| LAME | `1f5cc9487284d5950343aa5d4f70de433468070a` | [Bit packing](../thirdparty/ffmpeg/plugins/codecs/audio/mp3lame/0001-bitstream-use-unsigned-reservoir-shifts.patch), [Gain adjustment](../thirdparty/ffmpeg/plugins/codecs/audio/mp3lame/0002-quantize-avoid-negative-scalefactor-shift.patch) |

`git am` succeeded on pristine copies of all nine exact bases. The resulting
Git trees matched the authored trees. Application against current upstream
HEAD has **not** been checked; rebase and revalidate before submission if it
differs from the revision above. The FFmpeg snapshot includes a Meson build
port; these three changes touch only codec sources.

## Validation summary

- Linux, Clang 22, ASan + UBSan + LSan; PGO and LTO disabled.
- Full bundled configure/build with all twelve patches: exit code **0**.
- Test options: `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1` and
  `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`.
- Existing native Opus preview reproducer: **1/1 passed**, exit code **0**.
- Existing `pipeline.test_reencode_aom_av1_dovi_only`: **1/1 passed**,
  exit code **0**, including the libdav1d T.35 path that previously overread.
- Provider-focused test batch: **4/14 passed, 10 failed**, exit code **8**.
  The original diagnostics addressed by these patches did not recur; ten tests
  stopped at different, newly exposed sanitizer diagnostics listed below.
- Two additional HDR preview checks failed on existing playback deadline
  assertions without sanitizer reports. They are not Dolby Vision reproducers
  and are not evidence of a clean preview suite.

**This is not a clean-suite or release-readiness claim.** The passing tests
validate their named paths only. For the other patches, compilation, mail-patch
application and removal of the original report are verified, but successful
end-to-end completion is blocked by further defects. Keep that distinction in
any upstream submission.

### Further sanitizer blockers exposed after these fixes

These are separate follow-up findings, not fixes included in this archive.
They still require source-level investigation before proposing patches.

| Provider | New diagnostic | Existing reproducer |
|---|---|---|
| VMAF | Negative left shift in `adm_avx2.c:2927` | `pipeline.test_vmaf_picture_api_contract` |
| Kvazaar | Misaligned `int64_t` store in `quant-avx2.c:620` | `pipeline.test_transcoder_kvazaar_pads_unaligned_video` |
| x265 | Shift by 32 of a 32-bit value in `bitstream.cpp:62` | `pipeline.test_transcoder_hevc_encode_output_file_properties` |
| SVT-AV1 | Misaligned coefficient load in `convolve_avx2.h:326` | `pipeline.test_reencode_svt_av1_dovi_only` |
| SVT-AV1 | Misaligned store in `dlf_intrin_sse2.c:1921` | `pipeline.test_transcoder_svt_av1_encode_preserves_hdr10_metadata` |
| OpenH264 | Shift by 32 in `golomb_common.h:103` | Both OpenH264 encoder tests below |
| Vorbis | Array index 17 into `float[17]` in `psy.c:347` | `pipeline.test_opus_tags_preserved_in_vorbis` |
| FDK-AAC | Negative left shift in `fixpoint_math.h:857` | `pipeline.test_fdk_aac_decodes_surround_to_flac` |
| FDK-AAC | Negative left shift in `fixpoint_math.cpp:667` | `pipeline.test_fdk_aac_encodes_stereo_mp4` |

The proprietary NVIDIA driver leak and previously known preview/timing
failures remain outside these submissions.

## Suggested FFmpeg cover message

### Title

Fix three codec integration issues found with ASan and UBSan

### Body

This series fixes three independent issues reproduced while running existing
StormByte-Multimedia tests against a bundled, sanitizer-instrumented FFmpeg:

1. Mono hybrid Opus decoding forms a pointer relative to the absent second
   CELT output buffer. Initialize delay pointers only for active channels.
2. Dolby Vision T.35 payloads supplied by external decoders may lack trailing
   bitreader padding. Copy the payload into separately owned zero-padded
   storage before parsing the EMDF wrapper and RPU.
3. The OpenH264 bridge passes FFmpeg's constrained-baseline value, including
   constraint flags, as an OpenH264 enum. Translate it to `PRO_BASELINE`.

The changes do not modify packet formats or the intended profile policy.
The Dolby Vision copy remains separate from the parser's extraction scratch
buffer to avoid overwriting data while the EMDF bitreader still references it.
Allocation overflow and allocation failure are handled explicitly.

Validation: the full bundled sanitizer build succeeds. The existing mono Opus
preview and AV1 Dolby-only reencode reproducers both pass with ASan, UBSan and
leak detection enabled. OpenH264 gets past the original invalid-enum report,
but its encoder tests subsequently stop at a different shift-by-32 diagnostic
in `codec/common/inc/golomb_common.h:103`; no clean OpenH264 run is claimed.
The two affected tests are `pipeline.test_openh264_encodes_h264_matroska` and
`pipeline.test_openh264_encodes_ten_bit_after_format`.

Each fix is a separate commit and can be reviewed or applied independently.

## Suggested SVT-AV1 PR

### Title

AVX2: remove scalar alignment assumptions from level and coefficient loads

### Body

Level-neighborhood pointers are byte-addressed, and the two-tap coefficient
pair starts at `filter + 3`, which need not be four-byte aligned. Dereferencing
the current `int32_t`/`uint64_t` casts is undefined, even on x86.

Reuse `xx_loadl_32` and `xx_loadl_64`, and combine the loaded rows with unpack
operations in their original order. This preserves the load widths, lane order
and arithmetic while removing the alignment requirement.

The full bundled ASan/UBSan build succeeds. Existing HDR10 and Dolby Vision AV1
transcode tests no longer report the original loads at
`encodetxb_avx2.c:109` and `convolve_avx2.h:210`. They expose separate
misaligned accesses at `convolve_avx2.h:326` and `dlf_intrin_sse2.c:1921`,
so end-to-end sanitizer validation is still blocked by follow-up findings.

## Suggested Kvazaar PR

### Title

AVX2: use unaligned-safe loads for eight-pixel residual and prediction rows

### Body

Eight-pixel blocks are not guaranteed to be aligned for an `int64_t` lvalue.
Use `_mm_loadl_epi64` for the two residual inputs and the matching prediction
load instead of dereferencing scalar pointer casts. The intrinsic keeps the
same eight-byte access width and zeroed upper lanes.

The full bundled ASan/UBSan build succeeds. The existing unaligned-video
transcode test gets past the original misaligned read at `quant-avx2.c:534`.
It then stops at a separate misaligned output store at `quant-avx2.c:620`;
this patch does not claim that the entire quantization path is sanitizer-clean.

## Suggested x265 PR

### Title

encoder: avoid a null-source memcpy when allocating the first NAL buffer

### Body

On the first NAL, `NALList::m_buffer` is null and `m_occupancy` is zero.
The growth path nevertheless calls `memcpy` with that null source, violating
the library function's contract even though the copy length is zero.

Skip the copy when there are no existing bytes. Allocation, pointer fixups and
serialization are otherwise unchanged.

The full bundled ASan/UBSan build succeeds. The existing HEVC output-properties
test gets past the original null-source report. It subsequently stops at an
independent shift-by-32 diagnostic in `source/common/bitstream.cpp:62`.
This is a focused fix, not a clean sanitizer result for the whole encoder.

## Suggested Vorbis PR

### Title

psy: avoid undefined left shifts when packing negative Bark window bounds

### Body

The lower Bark-window bound can be negative for the first bins. Packing it
with `((lo - 1) << 16)` invokes undefined behavior. Use multiplication by
65536 instead; the supported window/block sizes keep the signed result
representable and preserve the intended packed value.

The full bundled ASan/UBSan build succeeds. The existing Opus-to-Vorbis metadata
test gets past the original negative-shift report at `psy.c:319`, then stops
at a separate array-bounds report at `psy.c:347`. Further investigation of
that report is still required; no clean encoder run is claimed here.

## Suggested VMAF PR

### Title

libvmaf: handle small AVX2 ADM coefficients before leading-zero calculation

### Body

The scalar ADM path only calls `get_best15_from32()` for coefficients at
least 32768. The AVX2 path invokes it for every lane and blends small
coefficients afterward. This can call `__builtin_clz(0)` or calculate a
negative rounding shift before the result is discarded.

Return small inputs unchanged with a zero shift before computing the leading
zero count. This follows the scalar behavior and leaves large-value rounding
unchanged.

The full bundled ASan/UBSan build succeeds. The existing identical-source
remux VMAF test passes with leak detection enabled. The picture API contract
test gets past the original zero/negative-shift reports but exposes a separate
negative left shift in `adm_avx2.c:2927`; it is not reported as passing.

## Suggested FDK-AAC PR

### Title

libFDK: widen signed fractional samples without shifting negative values

### Body

`FX_SGL2FX_DBL` shifts signed sample values by the difference in fractional
widths. Negative 16-bit samples make that operation undefined.

Multiply by the positive power of two instead. The full signed 16-bit input
range maps exactly into the signed 32-bit fractional range, so no saturation
or rounding change is introduced.

The full bundled ASan/UBSan build succeeds. Existing AAC encoding and decoding
tests get past the original conversion report from `fixmadddiv2_SD`.
They then stop at independent negative shifts in `fixpoint_math.cpp:667` and
`fixpoint_math.h:857`, respectively. Those are follow-up findings, not
sanitizer-clean results.

## Suggested x264 PR

### Title

threadpool: use type-correct worker callbacks instead of function-pointer casts

### Body

The pool invokes jobs through `void *(*)(void *)`, but its lookahead and slice
workers are passed through incompatible casts. The lookahead worker also has
a `void` return type. UBSan diagnoses these indirect calls.

Use the declared callback signature and convert the object pointer inside each
worker. Return null from workers whose result is unused and preserve the
encoder slice result. Apply the same contract to the input worker and the
pthread entry point, removing their unnecessary casts as well.

The full bundled ASan/UBSan build succeeds. The existing
`pipeline.test_software_decode_x264_encodes_h264_matroska` test passes with
strict UBSan and leak detection. Direct non-threaded slice calls retain their
existing return-value handling.

## Suggested LAME PR

### Title

Avoid undefined signed shifts in bit packing and subblock gain adjustment

### Body

This series contains two independent fixes:

1. `putbits2()` shifts a signed intermediate while packing output bytes.
   Convert the value to unsigned before the shifts so valid bit patterns do
   not overflow a signed `int`.
2. Subblock gain adjustment deliberately handles a negative `s`, but calculates
   the gain using a signed left shift. Replace it with bounded multiplication
   by the same positive scale factor.

The full bundled ASan/UBSan build succeeds. The existing
`pipeline.test_mp3_tags_roundtrip_through_opus` and
`pipeline.test_audio_cover_mp3` tests both pass with strict UBSan and leak
checking enabled. Each fix is provided as a separate mail-format commit.

## Evidence locations in the Multimedia workspace

These logs are not included in the submission archive; they remain local:

- `build/sanitizer-validation/ffmpeg-patches-configure.log`
- `build/sanitizer-validation/ffmpeg-patches-build.log`
- `build/sanitizer-validation/ffmpeg-patches-focused.log`
- `build/sanitizer-validation/ffmpeg-dovi-focused.log`
- `build/sanitizer-validation/provider-patches-configure.log`
- `build/sanitizer-validation/provider-patches-build.log`
- `build/sanitizer-validation/provider-patches-focused.log`
- `build/sanitizer-validation/provider-patches-focused.xml`

The unfinished Multimedia Transcoder OOM edits were preserved without staging
or including them in these twelve atomic integration commits.

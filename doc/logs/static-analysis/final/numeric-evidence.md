# Numeric-warning closure

This is the follow-up review of all **43 multiplication-widening** and **32 narrowing** warning emissions in the archived clang-tidy run: **75 emissions, 52 normalized source sites**. Paths and line numbers below identify the archived diagnostics. Current lines can differ; named expressions and current references resolve that ambiguity. No production files were changed, and no build or tests were run.

The analysis distinguishes exact bounded integer arithmetic, intended floating-point representation, and fixed test-input contracts. It does **not** infer safety from the warning category alone. Every one of these 75 TSV rows now references the corresponding evidence group below. No numeric site remains unreviewed; no new confirmed defect was established.

## Production arithmetic

### N01 — SimplePlayer storage/probe constants (2 emissions)

`lib/private/StormByte/multimedia/backend/simple_player.cxx:100,576`:

- `64 * 1024 * 1024` is exactly **67,108,864**, less than `INT_MAX` on this compilation target.
- The probe size `8 * 1024 * 1024` is exactly **8,388,608**.

Both expressions finish in `int` without overflow and are then represented exactly in `size_t`/`int64_t`. Their compile-time operands are the complete provenance; no input-dependent bound is missing.

### N02 — SimplePlayer timestamp conversion (3 emissions)

Same source, `:672,693,1041`: `Timestamp`, video frame duration, and subtitle packet-duration fallback.

The source is an FFmpeg `int64_t` timestamp/duration; `av_q2d(time_base)` makes the multiplication floating-point **before** it executes, so no signed integer multiplication overflow occurs. `Timestamp` handles `AV_NOPTS_VALUE` first, and the two duration paths require a positive duration. The destination contract deliberately uses `double` **seconds**, not exact integer ticks: see `display.hxx` fields `PreviewVideo::{m_position,m_duration}` and `PreviewSubtitle::{m_start,m_end}`, and the backend's `double` clock state.

An `int64_t` magnitude is within the finite range of `double`; a finite FFmpeg rational with nonzero denominator cannot push this product close to `DBL_MAX`. Integers above `2^53` can lose low timestamp bits when converted: that is an acknowledged property of this seconds-based representation, **not an exactness claim**. The warnings alone do not demonstrate a violation of the API's floating-point clock contract. Invalid zero-denominator metadata or stronger extreme-origin precision guarantees would require a separate input-contract finding; neither is established by these sites.

Classification: `REVIEWED_SECONDS_REPRESENTATION`, rather than falsely claiming lossless narrowing.

### N03 — Subtitle palette offset (1 emission)

Same source, `:1077`: `rectangle->data[1] + index * 4`.

`index` is loaded from `rectangle->data[0]`, whose FFmpeg type is `uint8_t*`, so it is in **[0,255]** regardless of the advertised palette count. It is rejected if it exceeds `nb_colors`. Thus `index * 4` is in **[0,1020]**, representable in unsigned arithmetic before conversion to pointer-offset width. This closes the multiplication warning; it does not assert that every other subtitle stride or provider allocation contract has been audited here.

### N04 — Degrain scratch reserve and confidence ratio (2 emissions)

`lib/public/StormByte/multimedia/pipeline/filters/video/degrain.cxx:372,435`:

- `RingSpan` is the local `constexpr size_t` value **5** (`:159`). In `64 * 2 * RingSpan`, the only `int` subexpression is `64 * 2 = 128`; the complete reserve request is **640**.
- `candidates` counts neighboring ring entries between `center - RingSpan` and `center + RingSpan`, excluding the center; consequently **candidates <= 10**. `accepted <= candidates`; the condition rejects `candidates == 0` before division.
- Every integer in this ratio is exactly representable in `double`, and the ratio intentionally compares against the floating confidence threshold `0.8`. No integer division or harmful precision loss is present.

## Fixed test arithmetic and conversions

### N05 — Corrupted-AVI iterator offset (24 emissions, one normalized site)

`test/test_helpers.hxx:212`, `WriteRejectedAviPacket`:

The 24 entries are the same header expression emitted through different include paths. The helper loads **only** `test/files/video/xvid_mp3_stereo.avi`, currently **437,684 bytes**, into `vector<char>`. It searches `movi`, then iterates only while `offset + 8 <= bytes.size()`. Before computing `bytes.begin() + offset + 8`, `TEST_REQUIRE(size <= bytes.size() - offset - 8)` also verifies the chunk payload bound. The assertion macro returns immediately on failure.

Therefore the converted `offset` is nonnegative and below 437,684, far below the target signed iterator `difference_type` limit. Callers vary the rejected packet kind/index, not the input fixture. This is a fixed-fixture proof, not a claim that arbitrary `size_t` values fit `ptrdiff_t`.

### N06 — Memory-writer iterator offset (1 emission)

`test/file/file_avio_test.cxx:128`, `SmallWriter::OriginPush`:

The writer is local to this test file. Its successful path writes the **24-byte** literal `abcdefghijklmnopqrstuvwx`, seeks to position **4**, and overwrites **4 bytes** with `WXYZ` (`test_file_avio_write_backpressure_and_seek`, `:232–253`). Other writer cases fail the origin/flush operation or use the same literal. `OriginPush` first grows the vector to `m_position + data.size()`. The converted iterator offset is therefore bounded by this 24-byte test buffer; no large position reaches this expression from an actual test call.

### N07 — Hardware-preflight dimensions (2 emissions)

`test/pipeline/video/video_test.cxx:142–143`:

The potentially unsigned dimensions are not arbitrary `Resolution` inputs. This block opens the fixed `video/hdr10_metadata_source.mkv`, obtains its first stream, and passes its parsed dimensions to `CheckHardwareEncoderCapability(int,int)`. `FFmpeg::MapVideo` in `lib/public/StormByte/multimedia/ffmpeg/property.cxx:421–426` creates the `uint32_t` resolution from `AVCodecParameters::Width/Height`, whose underlying FFmpeg members and wrapper accessors are **int** (`AVCodecParameters.cxx:129–142`). Thus any nonnegative decoded dimension already fits `int`; conversion to unsigned and back does not lose it. The hardware helper also rejects nonpositive dimensions at entry (`hardware_capability.hxx:199–202`).

This proof is restricted to this parsed fixture path; it does not promise that arbitrary public `Resolution(uint32_t,uint32_t)` objects fit a hardware C API.

### N08 — TokenReader fixture read length (1 emission)

`test/simple_player_test.cxx:440`, path-taking `TokenReader` constructor:

All path-taking callers use either `video/anime_like.mkv` (**4,000,912 bytes**) or `audio/noise_stereo.wav` (**384,078 bytes**); the separate binary-data constructor does not execute this `ifstream::read`. The source stream is opened with `ate`, so `tellg` returns the file-end position, then `seekg(0)` precedes the read. Both fixture lengths fit target `std::streamsize` and `size_t` exactly.

This closes the signed-width diagnostic for the supplied fixtures. Failed `tellg`/resource-exhaustion behavior is not used as a proof and remains outside this non-OOM scope.

### N09 — Literal buffer sizes (10 emissions)

The following integer products are fully constant and below `INT_MAX` before widening:

| Archived source sites | Expression/value |
|---|---|
| `test/file/duration_test.cxx:92` | `16 * 1024 * 1024` = **16,777,216** |
| `test/pipeline/ceiling_test.cxx:166,167` | `256 * 1024 * 1024` = **268,435,456** |
| `test/pipeline/ceiling_test.cxx:168,169` | `7680 * 4320` = **33,177,600** |
| `test/pipeline/filters/filter_test.cxx:430` | only the integer prefix `1024 * 2` = **2048** precedes `sizeof(int16_t)`; complete byte count **4096** |
| `test/pipeline/ocr/model_reader_test.cxx:36,47` | `320 * 80` = **25,600** |
| `test/preview_audio.hxx:159` | `16 * 1024 * 1024` = **16,777,216** before divisions by validated sample width/channels |
| `test/simple_player_test.cxx:710` | archived `2 * 6 * sizeof(float)` has integer prefix **12**, complete byte count **48**; current same assertion is near `:716` |

### N10 — Filter test PTS counters (2 emissions)

`test/pipeline/filters/filter_test.cxx:306,437`:

- `index * 1001` is fed by `index = 0..10`; maximum **10,010**.
- `index * 1024` is fed by `index = 0..7`; maximum **7,168**.

Both fit `int` exactly before conversion to the frame's `int64_t` PTS.

### N11 — Filter test tiny-frame row offsets (4 emissions)

Same file, `:396,469,506,507`:

All source images are explicitly allocated **16x16**, YUV420P or YUV420P10LE. Row counters are below 16 (below 8 for chroma). Reused/fresh GBR float images are explicitly scaled to **16x16**. They are CPU frames allocated by `AVFrame::AllocVideo -> av_frame_get_buffer`, not caller-supplied arbitrary strides.

The bundled FFmpeg `libavutil/frame.c:get_video_buffer` uses width-derived strides aligned to **32/64 bytes**, including the explicitly selected alignments. These formats and widths have at most 64-byte rows in these allocations, so `row * linesize <= 15 * 64 = 960`. Their products cannot overflow `int` before pointer-offset widening. This proof depends on the explicit CPU allocator/format/dimensions, not on all FFmpeg frames having small strides.

### N12 — NLMeans reference/fixture row offsets (3 emissions)

`test/pipeline/filters/nlmeans_plane_test.cxx:17,44,75`:

`ReferencePlane` is called only from the nearby explicit shape list `{1x1,1x7,7x1,9x6}`, depths `{8,10,12,16}`. The stride formula yields at most **24 bytes**. `row` is below the shape height, so the conservative bound is `abs(row * stride) <= 6 * 24 = 144`. The reversed-stride variant first moves the base to the last row and negates this same small stride (`:83–89`). The product stays in range, and the reversed pointers remain within the allocated fixture.

### N13 — Video-input fixture component and row offsets (6 emissions)

`test/pipeline/video_input/video_input_test.cxx:85,543,550,551,564,780`:

- `Samples`'s `column * step` uses `step` in `{2,4}`. Its actual callers allocate **64x64**, **5x3**, or the derived padded **8x8** / format-converted versions; all callers were enumerated. Even the loose bound `column < 64` gives product **<=252**.
- `test_nv12_repacking` allocates **5x3** with alignment **64**, deriving chroma dimensions **3x2**. The flagged row offsets in source/converted/restored images have `row <= 2`; CPU allocator strides are width-derived 32/64-byte aligned values, so these products are at most **128**.
- The final flagged padded-pixel component offset uses a known **8x8** destination and `step <=4`; the product is at most **28**. The row part at that expression is already explicitly widened to `ptrdiff_t`.

Negative-stride variants use the same tiny allocated planes with adjusted bases; the bounded component multiplication is unchanged.

### N14 — Watermark fixture row offsets (12 emissions)

`test/pipeline/watermark/watermark_test.cxx:116,171,172,200,385,386,429,430,455,456,493,494`:

All these accesses use the test's `constexpr PictureSize = 128`, not the original media dimensions. `SeedPicture` allocates a **128x128 GBR float** frame; `RenderComposite` captures that seeded image and converts it to the explicitly selected 128x128 format. `Before`, `After`, and the local linear frame retain that geometry. Formats are GBRPF32LE (four-byte samples) or YUV420P10LE (two-byte samples); CPU allocation and scale paths produce width-derived 32/64-byte-aligned strides.

For float planes the stride is **512** and the largest full-loop row is **127**, giving **65,024**. YUV rows/strides are smaller. The fixed SVG sample uses `y = Origin + 4 = 20`; its product is **10,240**. These integer products fit `int`; no arbitrary decoder stride is multiplied at the flagged fixture expressions.

### N15 — Preview test audio sizing (2 emissions)

`test/simple_player_test.cxx:224,634`:

- `FakeDisplay::Audio` receives frames only from SimplePlayer. The producer's `Samples` function rejects blocks for which **uint64_t(nb_samples) * channels * bytes_per_sample > 64 MiB** before constructing `PreviewAudio` (`backend/simple_player.cxx:922–964`). The accepted sample-width enum maps to `{1,2,4,8}` bytes in the consumer as in the producer. Dropping initial samples only decreases `m_frames`. Therefore the warned `m_frames * width` prefix is at most **67,108,864**, below `UINT32_MAX`; multiplication by channel count subsequently uses `size_t`. Positive-channel blocks are required by the consumer before this expression.
- Archived `:634` is `WriteAudioFixture`'s `vector<byte> expected(frames * channels * 2)` (current `:640`), not the unrelated expression now at line 634. All actual callers were enumerated: the normal fixtures use **48,000 Hz / 6 channels**, **48,000 / 2**, or **44,100 / 1** with `frames = rate/10`; the largest normal byte count is **57,600**. The new long cycling fixture explicitly requests **1,440,000 frames / 1 channel**, giving **2,880,000 bytes**. Every `uint32_t` intermediate is in range.

## Result and limitations

- **75/75 numeric emissions reconciled**, with exact-site references and contract evidence.
- **0 remaining numeric sites requiring an unknown bound** within these actual production/test paths.
- **0 newly confirmed residual numeric defects**.
- The three floating timestamp conversions are intentionally lossy representations, not proven exact conversions; their precision limitation is stated explicitly in N02.
- The 143 adjacent-parameter API-design warnings and the sibling-owned OOM/noexcept trace are unchanged.
- Verification here is source/provenance reasoning and artifact coverage checks only, not a fresh clang-tidy run or runtime validation.

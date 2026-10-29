# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Summary]

StormByte Multimedia turns media files into checked, asynchronous C++ jobs. Its fluent `Transcoder` API maps streams, remuxes or re-encodes them, applies typed filters and reports progress without requiring application code to call FFmpeg directly. When a workflow needs more control, the same pipeline stages can be connected manually.

The C++26 API includes codec and container registries, `File` inspection, owned `Plan`s, pipeline stages, filters, analytics, progress and telemetry. Backend codec and format contexts remain private; filter-facing frame, packet and graph adapters are available when needed. Cross-module values and lifetimes use StormByte Base's `Safe` APIs and require compatible provider/consumer ABIs.

The module uses [StormByte Base](https://github.com/StormByte-Suite/StormByte/releases/tag/2.2.2) 2.2.2, [StormByte Buffer](https://github.com/StormByte-Suite/StormByte-Buffer/releases/tag/2.0.5) 2.0.5, [StormByte Logger](https://github.com/StormByte-Suite/StormByte-Logger/releases/tag/2.0.5) 2.0.5 and [StormByte System](https://github.com/StormByte-Suite/StormByte-System/releases/tag/2.1.2) 2.1.2, plus FFmpeg, libvmaf, zimg, libebur128 and Tesseract/Leptonica with OCR language data. Dependencies may be bundled or selected from the system at build time; the resulting build determines which codecs and filters are available. This repository implements Multimedia only, not the other StormByte modules.

Original Multimedia sources are dual-licensed LGPL-3.0-or-later or commercial. Third-party dependencies and redistributed fixtures keep their own licenses; neither option grants codec patent rights. GPL/nonfree FFmpeg build options affect the resulting binary's redistribution requirements, not the license of the original Multimedia source.

- Module overview, build options and usage: [README.md](README.md)
- Original-source license: [LICENSE](LICENSE) and [COPYING.LGPLv3](COPYING.LGPLv3)
- Third-party notices and fixture provenance: [NOTICE](NOTICE) and [test/files/README.md](test/files/README.md)

## [Unreleased]

### Fixed

- Removed an unused Matroska muxer helper and avoided copying feature values in
  registry tests, clearing the corresponding Clang build warnings.

[Unreleased]: https://github.com/StormByte-Suite/StormByte-Multimedia/compare/1.0.1...HEAD

## [1.0.1] - 2026-10-29

### Added

- `Pipeline::Transcoder::Resources(ResourceUsage)` configures Full (the
  historical default), Medium (about half the CPUs available to the process)
  or Low (up to two) before `Run()`. Reduced modes distribute a best-effort
  thread budget across active decode/encode lanes, pass codec thread limits and
  reduce decoded-video frame queues while retaining a two-frame minimum.
  SVT-AV1 receives a corresponding supported parallelism level rather than a
  thread count; that level is not a hard CPU limit.
  Native thread settings are selected only when FFmpeg exposes the relevant
  option; x265 uses one frame thread with a bounded worker pool, and Kvazaar
  receives its supported `threads` parameter.
  FFmpeg libraries, filters, drivers and encoders without usable controls may
  use additional resources; this is not a hard CPU or RSS limit. `FineTune`
  thread-control keys conflict with Medium/Low and fail instead of bypassing
  the policy.
- `EncoderInfo::Hardware()` identifies implementations registered as
  hardware-only using their existing feature metadata. Optional hardware
  configurations do not make a software encoder hardware-only.
- Read-only, Safe-compatible `DecoderInfo` and `Codec::Decoders(need)` expose
  decoder names, descriptions and known input capabilities. Lists keep FFmpeg's
  backend default first, followed by available table-ranked alternatives;
  generic and subtitle defaults advertise no unverified features. Pins with
  mismatched codec ids, hardware-only prerequisites or experimental requirements
  are not advertised. Feature-aware read access uses the same eligibility checks.
- Decoder capability rows correctly advertise multichannel AAC, Vorbis, Opus,
  FLAC and ALAC input support, and use the actual `libfdk_aac` decoder pin.

### Fixed

- Audio and video decoders now warn and continue after a packet submission
  returns FFmpeg's recoverable `AVERROR_INVALIDDATA`, instead of cancelling the
  entire job. This handles rejected MP3 packets in older DivX/Xvid AVI files,
  including loudness measurement passes. Packet warnings identify the track,
  serial, size and FFmpeg error; other submission errors remain fatal.
- Reduced resource policies now apply to VMAF workers and its pending-input
  hopper, including when a larger worker count was explicitly supplied.
  Medium uses at most four libvmaf workers per video context and Low one,
  additionally sharing the job CPU target across video lanes and analysis.
  This substantially reduces libvmaf's resolution-dependent extractor memory
  without subsampling frames or changing the selected model. Reports expose
  the effective `threads` count. Filter frame caps, audio queues and compressed
  packet caps now also follow the selected resource policy, as do the PSNR,
  SSIM and XPSNR pending-input hoppers. Filter routes now receive the job Plan
  before their capacities and resource-sensitive operations are configured.
  Raw-video queues share a RAM-aware reservation across active video lanes
  and three codec/filter queues per lane, with conservative high-depth frame
  sizing and a two-frame minimum. The reservation is at most one eighth of
  available RAM, additionally capped to 4 GiB in Full, 1 GiB in Medium and
  256 MiB in Low. Analytics input no longer allows a 512-frame burst in Full:
  its maximum is 32 and is additionally memory-clamped. VMAF's extractor pool
  now has RAM- and resolution-aware limits in Full as well (at most 16 workers),
  with more conservative memory reservations in reduced modes.
  These are queue/extractor targets, not a hard RSS cap on native codecs or
  unpaired looks required by encoder latency.
- Reduced-resource SVT-AV1 encoding maps the per-stage target to a supported
  `lp` parallelism level instead of passing the thread count as the level.
  SVT-AV1 documents `lp` as a parallelism/buffering tier, not a processor
  count or hard thread limit; Full retains SVT's historical automatic level.
- Decoder discovery rejects video implementations whose advertised pixel formats
  are hardware-only, including FFmpeg's native `av1` decoder, which requires
  hardware acceleration despite not advertising the hardware capability flag.
  Software AV1 alternatives remain selectable and covered by real decoding tests.

- Windows Release builds no longer override the shared AVX baseline with a
  target-local `/arch:AVX2`. The override allowed clang-cl to emit Haswell-only
  instructions, including BMI1 `ANDN` in codec capability queries, causing
  `STATUS_ILLEGAL_INSTRUCTION` on Sandy Bridge and Ivy Bridge CPUs. Specialized
  dependency kernels retain their runtime CPU dispatch.

## [1.0.0] - 2026-10-28

First public release. The entries below describe the initial API, supported workflows
and their build, lifecycle and media-preservation guarantees.

### Changed

- Bundled builds use Base 2.2.2, Buffer 2.0.5, Logger 2.0.5 and System 2.1.2.

### Fixed

- The codec-parameters wrapper explicitly befriends the private FFmpeg format-context wrapper, avoiding Microsoft's unqualified-friend extension and accidental lookup of the global C type.
- Duration-scan pause, resume and cancellation tests use the throttled test reader so unknown-size scans cannot finish before their progress heartbeat and bypass the operation under test.
- BuildMaster is pinned to 2.1.3 and external installations require at least that version. Reconfiguring Release after Debug now refreshes the nested toolchain and builds bundled dependencies in the intended mode; Meson Release builds disable debug assertions.
- The default VP9 encoder worker count is bounded by the filtered output height and CPU availability, avoiding excessive worker synchronization and memory on smaller frames. Explicit `FineTune` thread counts are unchanged.
- Buffered FFmpeg input copies directly from the reader's unread FIFO bytes, avoiding an intermediate extraction buffer while retaining reader caching, telemetry, seek, retry and error behavior.
- Demuxed and encoded packet payloads use bulk byte copies instead of appending one byte at a time through the Safe binary iterator overload, reducing CPU overhead on high-bitrate remuxes.
- Mixed remux/audio-encode jobs no longer query available system memory for every demuxed packet. RAM-based queue sizing is only evaluated for stages that actually buffer decoded video frames; bounded packet queue capacities are unchanged.
- Pipeline packet diagnostics skip payload hashes and string formatting when low-level logging is disabled. Worker thread names are assigned during setup instead of once per packet.

### Added

- Encoder capability and settings API:
	- Public, DLL-exported `EncoderInfo`, `EncoderSetting`, `EncoderOption` and `EncoderOptionType`. `Codec::Encoders(Features)` lists the available encoder implementations that preserve the requested features, in the same preference order the pipeline uses, so `Encoders(need)[0]` is the implementation chosen by default. Each entry reports its name, description, features, which settings it supports (`CRF`, `BitRate`, `MaxBitRate`, `Preset`, `Tune`, `Params`) and the FFmpeg option behind each one. `EncoderInfo::Option()` describes an FFmpeg option (type, range, default and named choices) so applications can validate input before starting a job.
	- `Track::CRF()` takes a `double`. Video encoders with a floating-point quality option (x264, x265, NVENC `cq`) accept fractional values; encoders whose option is an integer (SVT-AV1, QSV, VAAPI) fail instead of rounding silently. Non-finite values fail.
	- Audio quality-based VBR through `Track::CRF()` for Vorbis (-1..10) and LAME MP3 (0..9). Audio `CRF` and `BitRate` are mutually exclusive.
	- Track settings are never dropped silently: `CRF`, `BitRate`, `MaxBitRate`, `Preset` and `Tune` fail the job when the track type or the selected encoder cannot apply them. `MaxBitRate` applies to video and audio; audio forwards `CRF` and `FineTune`, subtitles forward `FineTune`.
	- `FineTune` keys for encoders with a packed private option (`x264-params`, `x265-params`, `svtav1-params`, ...) are packed into it. Unknown keys fail the job when the encoder opens, including keys inside packed options that the FFmpeg wrapper would otherwise only warn about. The check reads the encoder's own log output, so it works with bundled and system FFmpeg alike. Passing the packed option itself as a key fails.
	- Without an explicit bitrate, audio encoders use their own defaults (for example Opus 64 kb/s per stream plus 32 kb/s per coupled stream, Vorbis quality 3). The source bitrate is never inherited, so a 1.5 Mb/s DTS track does not become a 1.5 Mb/s Opus target.
	- Video encoders require the bit depth of the first frame they receive (after filters): `TenBit`/`TwelveBit` join the required features, so automatic selection skips 8-bit-only encoders and a pinned one fails with a clear bit-depth error. Public `Format` accepts a `Property::PixelFormat`.
	- ASS, SSA and TTML subtitle encoders receive packed events (`ReadOrder,Layer,Style,...,Text`), so encoded Matroska ASS tracks keep valid cues.
- Codec queries:
	- Public, DLL-exported `Codec::SubtitleKind()` returns an optional `Text`, `Bitmap` or `Unknown` classification from FFmpeg codec descriptors, independently of encoder/decoder availability. Non-subtitle codecs return an empty optional; binary-packed text such as `mov_text` remains `Text`.
	- Public, DLL-exported `Codec::HasAccess(Access, Features)` checks whether an available encoder or decoder preserves all requested features. Read and Write are checked independently against private implementation tables and actual FFmpeg implementation availability. Encoder queries share the pipeline selector, so an H.265 build with only Kvazaar does not appear HDR10- or Dolby Vision-capable. An empty feature mask matches the generic access query.
	- Registry access mirrors what the pipeline can actually do. Codec `Read` requires an FFmpeg decoder and `Write` requires an encoder row the pipeline selector can open; codecs FFmpeg can only encode outside the pipeline tables (for example DTS) are not advertised as writable. Container `Write` requires a file-capable muxer resolved the same way the Plan resolves it, and `Container::Allows()` only lists codecs that muxer accepts at normal compliance (MP4 rejects VP8 and WebVTT, Matroska rejects TTML and 3GPP Timed Text).
	- Encoder feature tables audited against upstream FFmpeg (n9.0.2) for software (x264, x265, libaom-av1, libvpx, libvpx-vp9, SVT-AV1, AAC) and hardware (NVENC, QSV, VAAPI, VideoToolbox) encoders. With `EnforceFeatures` disabled, first-frame HDR needs only consider the preferred encoder row.
	- Matroska's compatibility catalog includes its native video codecs, including H.264, H.265, H.266, AV1, VP8, VP9 and FFV1.
- HDR and Dolby Vision:
	- Dolby Vision profile 7 sources (base layer + enhancement layer) can be transcoded. The RPU is rewritten as single-layer the same way `dovi_tool` mode 2 does: residual disabled, NLQ parameters dropped, identity reshaping for FEL and profile 8.1 colour coefficients; all dynamic metadata is kept. Output is profile 8.1 for HEVC and 10.1 for AV1. Remux copies all layers unchanged.
	- AV1 encodes carry HDR10+ (SMPTE ST 2094-40) dynamic metadata as an ITU-T T.35 metadata OBU after the temporal delimiter and sequence header, matching the HEVC SEI path. libaom-av1, which writes the OBU itself, is left untouched.
	- MP4 outputs write the Dolby Vision configuration box (`dvcC`/`dvvC`). Muxer compliance is relaxed only for outputs that carry a DOVI configuration.
	- Video encodes preserve the source chroma sample location (e.g. `topleft` in HDR10 and Dolby Vision masters).
	- Watermark opacity is applied in perceptual space (PQ for HDR, 2.4 gamma for SDR).
- Metadata, chapters and dispositions:
	- Chapters are read at open and copied to the output. Public, DLL-exported `Metadata::Chapter` (start, end and optional title on the source timeline) and `Metadata::File::Chapters()` expose them.
	- Public, DLL-exported `Metadata::File::StartTime()` returns the container start time (0 when unknown).
	- Public, DLL-exported `Pipeline::Muxer::Disposition(int)` getter and setter, alongside `Language` and `Title`. Streams without an explicit disposition keep the container policy's automatic default-track selection.
	- Transcoder outputs inherit each source stream's language, title and disposition (default, forced, hearing impaired, commentary, ...) for encoded and remuxed tracks. Per-track language and title overrides apply independently to each output.
	- Repeated `Transcoder::Video()`, `Audio()` and `Subtitle()` mappings create independent outputs from the same source stream. Keep an original bitstream by remux while also encoding a compatibility track (for example DTS-HD MA plus E-AC3), encode several variants with separate settings and filters, or create multiple remux copies. Each branch receives the complete input; metadata overrides, analytics and settled encoder callbacks identify their own output slot. Manual pipelines can select an explicit remux output slot with `Remuxer(logger, in, out)`.
	- Encode and remux copy the source title, artist, album, album artist, composer, genre, comment, copyright, date, track and disc numbers to supported destination metadata. The output's `creation_time` is the encode time (UTC), written as Matroska `DateUTC` and the MP4 header creation time. Supported encoder/application fields are branded `StormByte-Multimedia <version>` after muxer initialization; backend technical vendor fields such as the MP3 Xing/LAME identifier are kept.
	- `File::Metadata()` falls back to stream comments for missing fields in single-audio files, so OpusTags and Vorbis comments are readable through the typed API. Container tags take precedence.
- Timeline and job control:
	- Encoded video and audio keep the source timeline, including negative B-frame DTS and audio encoder priming. MP4 output rebases the timeline to the source start time, as the FFmpeg CLI does, so priming is trimmed by the edit list; chapters follow. Matroska keeps the source timeline and signals priming with `CodecDelay`.
	- Remuxed packets without timestamps (laced Matroska audio such as TrueHD) are given PTS and DTS by the muxer. Equal consecutive DTS are kept where the output format allows them (`AVFMT_TS_NONSTRICT`), and a filled-in DTS never exceeds the next decode slot.
	- `Transcoder::Pause()` halts every pipeline stage at a shared gate; `Resume()` releases it and `Cancel()` ends a paused job immediately. A plan built while the job is paused starts held.
	- Duration calculation honours pause, resume and cancellation, including cancellation while paused and scans of readers with an unknown size. Interrupted scans rewind the input buffer and are not cached as completed durations, so they can be retried.
	- Progress uses a positive caller-supplied duration first, then the container's duration estimate, and scans the full source only when neither is usable. Transcoding without a supplied duration avoids a full pre-scan when the container already reports one. `File::DurationHint()` reads the available duration without scanning; explicit `File::Duration()` queries retain their scan and cache behavior.
	- Muxed packets are interleaved by timeline, not arrival order: the container holds packets per track and writes the oldest once every audio and video track has one queued, keeping audio and video together despite encoder lookahead (for example SVT-AV1 at 4K). Subtitles never gate, and a track silent for more than 60 s of timeline stops gating. The demuxer park follows the Plan's ceiling, so remuxed tracks cannot run ahead of a slow encode.
	- `Progress::Main` is the real transcode percent: it follows the muxer's written position and is exactly 100 once the trailer is written. Measure and analytics never contribute to it; `All` keeps the weighted sum of every mounted phase. `MainComplete` marks completion of the main phase.
	- Two-pass filters (for example loudnorm) switch from measuring to processing on the first frame of the second read, not when the demuxer reaches EOF. Frames still parked in the demuxer, queued in the decoder or held by slower upstream filters are measured before processing starts, so audio is neither shifted nor duplicated.
	- Two-pass measurement reaches only the decoder branches that need it, leaving sibling remux and one-pass outputs untouched until processing. Source rewind keeps FFmpeg's AVIO buffer synchronized with the reader, avoiding duplicated buffered packets on the second pass.
	- Cancellation preserves analytics-stage lifetimes through teardown. SVT-AV1 encoders receive EOS before teardown even when cancellation skips the normal pipeline flush. Pending packets are discarded if needed to accept EOS; the codec's close routine still owns draining and releasing its resources.
	- Pipeline threads format and parse numbers with the "C" locale, independently of the host application's locale.
	- Pipeline logging keeps a separate Multimedia scope for each application logger. Duration scanning and encoding retain their job's destination, and scoped stage loggers keep their sibling component paths.
	- Opus encodes of 3–8 channel sources with a non-Vorbis channel order (e.g. 5.1(side)) are converted to the matching Vorbis order with the resampler; channel content is unchanged.
- Build and release:
	- `ENABLE_PGO` and `ENABLE_LTO`. Profile-guided optimization trains in `pgo-instr` and installs this build; link-time optimization follows `ENABLE_LTO` (default ON) for every build type. Release builds append `-O2` / `/O2` and the CPU baseline only when the caller did not set flags (`-stdlib=` alone does not count), and never pass `/fp:fast`. Windows Release uses `/arch:AVX`.
	- `THIRD-PARTY` and the extra `COPYING.*` texts for components compiled into the shared library. Release archives are built with every media dependency `BUNDLED`, `WITH_GPL=OFF` and `WITH_NONFREE=OFF`. Linux ships `libstdc++` and `libc++` archives, macOS ships Apple Clang `macos-arm64`, Windows has no runtime suffix.
	- Requires [StormByte Base](https://github.com/StormByte-Suite/StormByte/releases/tag/2.2.1) 2.2.1, [StormByte Buffer](https://github.com/StormByte-Suite/StormByte-Buffer/releases/tag/2.0.4) 2.0.4, [StormByte Logger](https://github.com/StormByte-Suite/StormByte-Logger/releases/tag/2.0.4) 2.0.4 and [StormByte System](https://github.com/StormByte-Suite/StormByte-System/releases/tag/2.1.1) 2.1.1, each as a release archive linked with `BACKEND=binary`. BuildMaster is >= 2.1.3.
	- Multimedia exceptions, logs, filter graphs, durations and reports format through `Safe::String::ReplaceToken`. Exception paths are segments: `StormByte.Multimedia`, then `File`, `Codec`, `Plan`, `AV`, `AV.Decoder` and `OCR`.
	- Bundled dependencies build with Clang, clang-cl, GCC, MSVC and Apple Clang, with LTO and PGO. Bundled static FFmpeg archives are linked directly (Linux wraps them in a rescan group). CI builds with `ENABLE_LTO=ON` and `ENABLE_TEST=ON` and runs ctest.
	- Shared Multimedia builds keep private FFmpeg and codec archives out of test executable link interfaces. Tests that call FFmpeg directly share one test library; the OCR model test declares its private OCR dependencies explicitly. Static Multimedia builds retain their transitive implementation dependencies.
	- Bundled zlib follows the requested FFmpeg library type. Windows static builds link the static zlib archive without introducing a runtime `z.dll` dependency.

- Media inspection and processing API:
	- Add codec/container registries with names, aliases, extensions, operations and compatibility data. `File` inspects streams, languages, titles, attachments, duration, audio layouts and video properties, including HDR10 and signaled HDR10-like metadata.
	- Add owned `Plan` inputs/outputs and asynchronous `Demuxer`, `Decoder`, `Remuxer`, `Encoder` and `Muxer` stages. `Transcoder` provides stream mapping, remux or encode selection, quality and metadata controls, attachment filtering, lifecycle hooks, pause/resume/cancel and reports.
	- Add generic libavformat muxing for registered single-file destinations, with Matroska, WebM and MOV/MP4 policies selected from the actual output path. Preserve per-track packet order, source language/title by default and valid signed timestamps; truncate destinations before replacement. Formats requiring multiple output files remain outside the buffered-writer workflow.
	- Add audio/video frame filters, packet filters, two-pass processing and independent analytics routes. VMAF reports per-track mean/minimum scores and scored-frame counts; regional Degrain is explicitly experimental and heuristic, not motion-compensated restoration.
	- Add bitmap subtitle OCR through Tesseract, including language-aware PGS-to-SubRip conversion with bundled or system OCR language models. Attachment inclusion is explicit; attachments are dropped unless requested.
	- Select language models independently of the OCR engine through `WITH_TESSDATA=BUNDLED/SYSTEM`. Bundle selected `tessdata_best` LSTM models for European and major Asian subtitle languages plus Arabic, Hebrew, Turkish and Persian, with base identifiers maintained in the independent tessdata component's CMake configuration and vertical variants included automatically. Load system models directly from the discovered directory without embedding them; accept partial installations and reject absent requested languages at runtime without fallback.
	- Load secondary Tesseract language models, including vertical variants requested by the primary model, from the embedded catalog rather than the filesystem. Resolve model paths with Unix or Windows separators and retain exact-language selection without fallback.
	- Keep bitmap subtitle OCR in raw pixels from FFmpeg decoding through grayscale input and bundled Tesseract's internal image storage. Add white image margins for tightly cropped glyphs, including Japanese subtitle recognition. Disable bundled Tesseract's TIFF-backed diagnostic fonts and PNG image serialization so Leptonica does not require external image codecs for this workflow.
	- Add automatic supported-layout adaptation for 7.1-to-5.1 AC-3/E-AC3 encoding. MP3 encoding with more than two input channels requires an explicit downmix.
	- Add progress snapshots and retained stage/job telemetry for media counts, operation timing, waits and sampled process RSS.
	- Add `Property::Space::IPTC2` for FFmpeg's IPT-PQ-C2 matrix coefficient and Dolby Vision Profile 5 signaling.
	- Add bundled Kvazaar and OpenH264 encoder entries while retaining x265/x264 preference where configured.
- Add bundled NVIDIA codec headers `n13.1.15.0` and enable H.264, HEVC and AV1 NVENC in bundled Linux/Windows builds without requiring the CUDA Toolkit at compile time. Enable FFmpeg's dynamic CUDA driver device support; keep NVDEC and CUVID disabled in Multimedia. Advertise only verified capabilities: HEVC/AV1 10-bit and static HDR10, with no HDR10+ or Dolby Vision claims and no H.264 10-bit claim.
- Add design-resolution scaling to Watermark. Empty keeps authored size; one zero axis follows the frame's corresponding dimension, and `0x0` disables scaling. Anchor margins scale uniformly while absolute points remain destination pixels. Preserve source pixels and straight alpha; alter only the logo rectangle, keep fully transparent pixels untouched, and map SDR white to 203 cd/m² for PQ/HLG without retagging the frame.
- Add exact video-input negotiation after selecting the feature-compatible encoder. Preserve supported CPU formats and shared pixel buffers; when necessary, repack only little-endian `yuv420p10le`/`p010le` without changing samples, depth, chroma, color, geometry, timing or native/wrapper HDR and Dolby Vision metadata. Retain prepared frames across `EAGAIN`, reuse compatible hardware pools or perform exact uploads/downloads when the encoder advertises frames-context support. Unsupported representations and devices fail explicitly without lossy conversion or software fallback.
- Add `Drop`, `EmitFrame` and `EmitGraph` for custom buffering frame filters while keeping `Save` as a one-for-one replacement. Graph adapters expose non-closing `Receive`; native frames expose their timestamp timebase.

- Add the manual pipeline Plan API and lifecycle:
	- `Plan` is noncopyable/nonmovable and retained through `Safe::Shared<Plan>`. Configuration, `Check()` and `Tracks()` are public; snapshot/container/location/path accessors are private. State, error and I/O telemetry remain observable, with no public state setter.
	- Add shared `Created -> Running -> Finished` state with terminal `Failed`, first-failure preservation and no failure after `Finished`. Constructors still start workers; configure and wire the graph, then close filters before binding the Plan to publish `Running`. Finish only after all stages and analytics complete.
	- Require stages to outlive execution, propagation and notifications; stop peers before joins and serialize state publication with teardown.
	- Accept registered `const Container&` overrides for Plan and Transcoder, independent of the output suffix. The Plan borrows the registry identity, which must outlive it.
- Select muxers from the output path through FFmpeg's format support, with explicit policies for Matroska, WebM and MOV/MP4 and clear failures for unsupported combinations.
- Keep optional codec, HDR/Dolby Vision and Japanese OCR cases registered across dependency/license configurations. Skip missing write support or unavailable pinned implementations; skip bundled FDK-AAC cases before pipeline startup when nonfree support is disabled. Run Japanese OCR recognition checks with both bundled and system OCR; FDK-AAC availability is optional.
- Exclude GPL-dependent filters from compilation and header installation when `WITH_GPL=OFF`; Rubberband and VagueDenoiser require GPL. The current filter set has no nonfree-dependent filters; keep the independent Hqdn3d implementation available without GPL.

- Build and dependency consistency:
	- Enable zimg's architecture-specific dispatch, including x86 AVX512 variants and ARM NEON. Explicitly request bundled SIMD implementations and runtime CPU selection where supported, require standalone FFmpeg x86 assembly, and keep higher instruction sets confined to specialized kernels, including per-file ISA options for MSVC and clang-cl on Windows. Disable Opus fuzzing instrumentation and optimized-versus-C comparison checks in bundled builds.
	- Link video-content tests against the bundled FFmpeg pack when available, including platform dependencies such as Windows BCrypt. Link plugin archives through normal symbol extraction rather than whole-archive forcing, preserving registered FFmpeg components while avoiding duplicate Vorbis objects and Windows import descriptors.
	- Compile bundled consumers against matching source-checkout headers for FFmpeg, VMAF, zimg, libebur128, Tesseract, Leptonica, tessdata, StormByte Buffer and pthread-Win32. Prepend these include paths before dependency installation only for bundled builds; leave system selection unchanged.
	- Align `WITH_VMAF` with `WITH_FFMPEG` before dependency selection and report cache overrides, preventing mixed bundled/system ABIs. Disable IPO for all x265 bit-depth variants; keep x264 outside GCC LTO to preserve stack realignment for dependent SIMD code.
	- Use default pthread attributes for bundled Kvazaar workers on MSVC.
- VMAF and media metadata:
	- Compile bundled VMAF against headers matching its library. Cover 8-bit/10-bit allocation, pool, read, flush and scoring APIs for both dependency modes.
	- Treat VMAF picture preparation, read, flush and score-pooling errors as terminal; stop draining after failure and report runtime version and picture diagnostics. Allow muxer encoder-readiness waits to finish when input reaches EOF.
	- Preserve native demuxed packet properties for ordinary and look decoders; detect native decoded HDR10+ in `File`. Preserve buffered raw-stream packets when restoring metadata without timestamp seeking, and rewind before opening demux input.
	- Require HDR10+ when selecting an HDR-preserving encoder for HDR10+ input. Declare libaom's native AV1 HDR10+ path and the container-conditional libvpx/native VP9 Matroska side-data path without claiming VP9 bitstream support. Add a fixed 48-frame fixture and per-frame T.35, timing, static HDR and HEVC -> AV1 -> HEVC round-trip checks, including native decoding of final elementary HEVC without container metadata; require all 48 canonical payloads and exact color/static metadata without packet-level HDR10+ injection.
	- Declare static HDR10 signaling/preservation for libaom AV1 and HDR10 signaling for SVT-AV1. Preserve replacement-frame HDR/Dolby Vision metadata without overwriting recalculated HDR10+ or duplicating DOVI entries; initialize look-decoder Dolby Vision from effective packet parameters, not stale source HEVC profile. Check AV1 profile 10 metadata without requiring HEVC-format raw RPU buffers, and allow only AV1 representation quantization during HEVC-to-AV1 static metadata comparisons.
	- Classify Dolby Vision Profile 5 IPT-C2/PQ separately from HDR10; HDR10 heuristics require a BT.2020 NCL/CL matrix as well as BT.2020 primaries and PQ transfer.
- Muxing, codecs and packet flow:
	- Implement buffered AVIO backpressure handling: preserve temporary reader unavailability as EAGAIN rather than EOF and rearm retained AVIO EAGAIN before the next packet read. Flush pending writer bytes and split oversized output blocks within the callback without loss or duplication, while preserving terminal I/O errors.
	- Keep remux input indices separate from encoder output indices during reservation/routing; respect reservation failures before publishing connections. Preserve packet properties and side data, including late libaom AV1 extradata and audio skip samples, by copying after payload allocation. Clone remux codec parameters after short sources reach EOF so subtitle-only outputs can write headers.
	- Prepare subtitle encoders during mux reservation so real codec parameters/extradata exist before the first cue, without substituting unopened encoders with SubRip at flush time. Preserve audio/video decoder errors, support delayed subtitle tracks, and supply MOV timed-text encoders with ASS rectangles while retaining cue timing.
	- Adapt JPEG/PNG/BMP attachments to MOV/MP4 cover-art packets, retain Matroska serialization and existing `File` classification, and reject unsupported MP4 attachments. Implement Watermark setup for path and binary overlays.
	- Prevent Demuxer setup from missing a bound Plan; allow decoder initialization after short-source EOF while context and queued packets remain available. Create planned per-track hopper connections before generic transfer, reserve manual encoder mux streams through typed `Encoder >> Muxer`, propagate errors at mux close, and apply interleave limits consistently to MP4 and generic muxers.
	- Honor explicit decoder pins from Transcoder and manual Plans; missing or mismatched implementations fail rather than falling back. Format implementation labels and encoder/muxer errors as text; require an initialized private opened-encoder result.
	- Normalize unspecified PCM input/output layouts consistently with libswresample without mutating source samples; retry audio-frame and EOF submission after receives that consume input without producing packets. Peek packet payloads without advancing FIFO cursors so `EAGAIN` retries resubmit original data.
	- Retry video decoder EOF submission after draining pending frames on `EAGAIN`. Preserve remuxed average frame rate so Matroska retains source `DefaultDuration`.
- Pipeline lifecycle and filter correctness:
	- Create typed connections before first transfer, propagate terminal stage failures even when filters are idle, abort shared hoppers on cancellation/failure to wake blocked producers, and cancel peers before joins. Discard parked demux packets on failure but retain ordered normal EOF draining; final demux destruction joins feeders without accessing retired owner synchronization.
	- Install route audit consumers before producers, synchronize clone registration with emission/EOF, propagate EOF to late clone destinations, and apply route capacities after wiring so queues remain bounded. Destroying a filter graph cancels endpoints, filters and look decoders before joining workers.
	- Keep frame metadata compatible with transformed colorimetry: inherit unspecified tags for pixel-only replacements, let explicit output tags win, and retain/restore HDR10/HDR10+ only for compatible output and Dolby Vision only while source color encoding remains unchanged. Remove incompatible native/wrapper metadata and attachments so encoders cannot restore stale tags; keep SDR color signaling even on encoders without HDR support.
	- Return genuinely empty `AVFrame`s on graph `EAGAIN`/EOF; consume retained inputs instead of passing them through; drain all available outputs and emit delayed EOF frames with retained input lineage. Carry decoder/source/sink timebases and end timestamps through graph processing while preserving output timing and unique parts.
	- Use valid BM3D block size. Resolve Decimate cadence from stream metadata or a timestamp/duration first pass without caller FPS, and propagate the negotiated output cadence. Initialize delayed Yadif/Bwdif frames as empty and emit buffered output at EOF.
	- Translate FFmpeg chroma-position enums to zimg values and include input range, matrix, transfer, primaries and chroma position in cached-conversion keys. Use reusable per-filter `AVFrame::ScaleContext` caches, released during ordinary teardown, and explicit contexts for direct repeated scaling.
	- Reuse rolling integer patch sums in NlMeans without changing search, borders, strength, weights, rounding or thread count. Preserve Degrain's documented heuristic limitations.
	- Patch FFmpeg's MOV/MP4 track-name reader to accept raw UTF-8 and avoid false UDTA length warnings; system FFmpeg is unchanged. Drop remaining HEVC slices when a picture's first slice is skipped to avoid stale reference-list errors in frame-threaded decode.
	- Cover filter output properties, bounded EOF/sample conservation, FPS frame counts/timestamps, zimg cache comparisons and SDR color tags. Verify TTML/MP4 support and explicit TTML/Matroska rejection.
	- Keep optional Rubberband/VagueDenoiser tests free of omitted class references and use VP9 instead of GPL-only x264 for general filter tests when GPL/nonfree features are disabled.

- Register CTest cases by build/dependency/license configuration, with category-specific executables sharing compiled helpers and bounded waits. Cover registries, `File`, facade/manual pipelines, remux/transcode, attachments, HDR, OCR, analytics, codec combinations, filters and invalid input/configuration.
- Cover audio/video implementation selection and roundtrips, decoder pins, short subtitle tracks, generic and MP4 mux policies, track order/language, aliases, mixed remux/encode, image cover-art roundtrips, unsupported attachments and audio-layout adaptation.
- Cover repeated source mappings with remux/encode in either order, multiple encodes or remux copies, nonzero source indices, independent metadata and downmix filters, one or several two-pass branches, complete packet/sample delivery and separate per-output frame-count/VMAF analytics.
- Check VP9/HEVC remux analytics against all 48 fixture frames; cover HDR10/HDR10+ metadata across software and available hardware encoders, Dolby Vision transforms, OCR and exact fixture provenance.
- Hardware tests preflight device/profile availability. Skip proven absent devices or unsupported backend capability, but keep invalid formats, allocation failures and generic encoder failures as failures. Test VAAPI SDR HEVC Main independently of Main10.
- Exercise Watermark from both path and `#embed` bytes; `File` verifies generated streams while visual appearance remains a manual check. Tests use Multimedia APIs in-process, do not invoke external FFmpeg programs, and document synthetic fixture/font provenance separately.
- Include synthetic-noise MP3 and Opus fixtures with placeholder music tags, documented authoring commands and checksums.

[1.0.1]: https://github.com/StormByte-Suite/StormByte-Multimedia/compare/1.0.0...1.0.1
[1.0.0]: https://github.com/StormByte-Suite/StormByte-Multimedia/releases/tag/1.0.0

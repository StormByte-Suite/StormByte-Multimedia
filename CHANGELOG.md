# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Summary]

StormByte Multimedia is the media inspection and processing module of the StormByte C++ suite. It provides a C++26 pipeline for demuxing, decoding, filtering, encoding and muxing through FFmpeg libraries, with a fluent `Transcoder` facade and the same stages available for manually wired pipelines.

Public headers under `StormByte/multimedia/` cover codec/container registries, `File` snapshots, stream and attachment metadata, media properties, pipeline plans and stages, filters, analytics, progress and telemetry. Backend codec and format contexts remain private; filter-facing frame, packet and graph adapters are exposed where needed. Owned cross-module values and lifetimes use StormByte Base's `Safe` APIs, with compatible provider/consumer ABIs and providers kept loaded while their objects remain alive.

The module depends on StormByte Base, Buffer, Logger and System, and on FFmpeg, libvmaf, zimg, libebur128 and Tesseract/Leptonica with OCR language data. Bundled and system dependency choices are build-time configuration, not a guarantee that every encoder or filter is available. This repository does not implement the other StormByte modules.

Original Multimedia sources are dual-licensed LGPL-3.0-or-later or commercial. Third-party dependencies and redistributed fixtures keep their own licenses; neither option grants codec patent rights. GPL/nonfree FFmpeg build options affect the resulting binary's redistribution requirements, not the license of the original Multimedia source.

- Module overview, build options and usage: [README.md](README.md)
- Original-source license: [LICENSE](LICENSE) and [COPYING.LGPLv3](COPYING.LGPLv3)
- Third-party notices and fixture provenance: [NOTICE](NOTICE) and [test/files/README.md](test/files/README.md)

## [Unreleased]

### Changed

- Scale a Watermark when a design resolution is supplied. Empty keeps the authored pixel size. One zero axis selects the other (`0x1080` follows frame height, `3840x0` follows frame width) and `0x0` disables the overlay. Anchor margins use the same uniform factor; absolute points stay in destination pixels. The encoded source is kept and `Clean` drops the scaled cache. Only the logo rectangle is rewritten, fully transparent pixels stay untouched, and PQ/HLG maps SDR white to 203 cd/m² without retagging the frame. Planar RGB conversions keep a straight alpha plane.
- Negotiate video input from the selected, feature-compatible encoder before opening it. Keep supported CPU formats and shared pixel buffers unchanged; permit only sample-exact little-endian `yuv420p10le`/`p010le` repacking when required, without changing encoder-selection defaults. Prepare independent frame headers, preserve color/timing and native or wrapper HDR/Dolby Vision metadata, and retain the same prepared input across `EAGAIN` retries. Reuse compatible hardware pools or create devices and pools for exact uploads/downloads where the encoder advertises frames-context support. Reject unsupported representations or devices explicitly without quality-reducing or software-encoder fallback. Cover format, stride, sample-roundtrip, source-immutability, attachment, hardware transfer and backpressure behavior.
- Keep optional codec, HDR/Dolby Vision and Japanese OCR cases registered across dependency and license configurations. Skip codecs without write support or unavailable pinned implementations, skip FDK-AAC before starting its encoder or decoder tests when bundled nonfree support is disabled, and skip Japanese OCR with system OCR independently of FFmpeg. Remove the bundled FDK-AAC availability requirement.
- Exclude filters requiring GPL or nonfree components from compilation and header installation when their respective build option is disabled. Rubberband and VagueDenoiser require `WITH_GPL`; the current filter set has no nonfree dependencies. Keep the independent Hqdn3d implementation available without GPL.
- Add explicit `Drop`, `EmitFrame` and `EmitGraph` operations for custom buffering frame filters, while keeping `Save` as a one-for-one replacement. Graph adapters expose non-closing `Receive`, and native frames expose their timestamp timebase.
- Breaking manual pipeline Plan API and lifecycle:
	- Make Plan noncopyable and nonmovable, retain it through `Safe::Shared<Plan>`, and remove value binding and `Clone`/`Move` requirements for derived Plans. Keep public configuration, `Check` and `Tracks()`; make snapshot, container, location and path getters private while retaining State, Error and I/O telemetry observation. There is no public state setter.
	- Introduce shared `Created -> Running -> Finished` execution state with terminal Failed, first-failure preservation and rejection of failure after Finished. Constructors still launch stage workers, but setup waits for Running; complete wiring and `Filters::Close` before Plan binding, which publishes Running. The library publishes Finished when participating stages complete, and Failed on the first formation or execution error. Successful completion requires all stages and analytics, not just muxer EOF.
	- Require stages to outlive execution, propagation and notifications, with peer cancellation before worker joins and externally serialized state publication and teardown. Migrate manual tests to shared Plans and add privacy, transition, failure-preservation and cancellation checks in existing sources.
	- Accept explicit registered `const Container&` overrides for Plan and Transcoder destinations, independent of filename suffix. Retain borrowed registry identity rather than copying it; the entry must outlive its Plan. Existing manual and facade tests cover unknown and conflicting suffixes.

### Fixed

- Link the video-content regression tests against the bundled FFmpeg pack when available, retaining its platform dependencies such as Windows BCrypt instead of linking only the individual FFmpeg archives.
- Link bundled FFmpeg plugin archives through normal symbol extraction rather than whole-archive forcing. Preserve FFmpeg's explicitly registered codecs and filters while avoiding duplicate Vorbis objects and import descriptors in Windows test executables.
- Compile bundled VMAF consumers against the public headers from the same source checkout as the bundled library, available before dependency installation. Prevent clean builds from picking up system headers with a different `VmafPicture` layout, which caused first-pair `EINVAL` failures and memory corruption. Add an 8-bit and 10-bit allocation, pool, read, flush and scoring API regression for both dependency modes.
- Prefer the matching source checkout headers for every other bundled third-party library consumed by Multimedia: FFmpeg, zimg, libebur128, Tesseract, Leptonica, tessdata and StormByte Buffer. Those include directories are prepended only for `BUNDLED`, so a clean build cannot take system headers of a different layout, and `SYSTEM` selection is unchanged.
- Treat VMAF picture preparation, read, flush and score-pooling errors as terminal failures instead of silently skipping pairs; stop draining after failure and report runtime version and picture diagnostics. Allow muxer encoder readiness waits to end when their input reaches EOF.
- Retain native demuxed packet properties and preserve them when submitting video to both ordinary and look decoders, and detect native decoded HDR10+ in `File` probes. Preserve buffered raw-stream packets when wrapping restored metadata without a timestamp seek, and rewind the reader before opening demux input. Require HDR10+ when selecting an HDR-preserving encoder for an HDR10+ source. Declare libaom's native AV1 HDR10+ metadata path and the container-conditional libvpx/native VP9 Matroska side-data path, without claiming VP9 bitstream support. Add a fixed 48-frame static HDR10/HDR10+ fixture and in-process per-frame T.35, timing, static metadata and HEVC -> AV1 -> HEVC round-trip checks, including final native HEVC decoding without container metadata. Drain the native decoder through EOF instead of remuxing timestamp-less raw HEVC into Matroska; require all 48 canonical payloads, exact static HDR and color properties, and no packet-level HDR10+ metadata injection. See [capability justifications](README.md#hdr10-metadata) and [fixture provenance](test/files/README.md#hdr10).
- Preflight hardware encoder device and profile availability in tests, skipping proven unavailable hardware or unsupported encoding capability without masking invalid formats, memory failures or generic encoder errors. Check VAAPI SDR HEVC Main independently of HDR10 Main10. Promote first-frame static HDR metadata before encoder opening without replacing metadata already present in the context.
- Align `WITH_VMAF` with `WITH_FFMPEG` before selecting the VMAF dependency, overriding mismatched values in the CMake cache with a `STATUS` message instead of mixing bundled and system library ABIs. Add configuration sanity coverage for both matching modes, both override directions and default VMAF selection.
- Declare libaom AV1 HDR10 support for color signaling and static metadata preservation through the pipeline and Matroska. Exercise every declared HDR10 encoder explicitly, and extend Dolby Vision reencode, watermark and pixel-replacement checks to SVT-AV1 and libaom, including combined HDR10 metadata and regenerated AV1 profile 10. Initialize look-decoder Dolby Vision configuration from effective packet codec parameters instead of inheriting the source HEVC profile. Check parsed AV1 Dolby Vision metadata without requiring HEVC-format raw RPU buffers, and account for static HDR metadata quantization only when converting HEVC to AV1.
- Skip license-excluded Rubberband and VagueDenoiser tests without referencing their omitted classes, and use VP9 instead of the GPL-only x264 encoder for general video-filter tests so permitted filters still run with `WITH_GPL=OFF` and `WITH_NONFREE=OFF`.
- Inherit unspecified color tags when replacing video pixels before checking HDR/Dolby Vision compatibility, preserving metadata for pixel-only replacements while keeping explicit SDR conversions authoritative.
- Reuse rolling integer patch sums in NlMeans instead of recomputing each patch for every candidate pixel. Preserve the search window, independently clamped borders, strength, weights and rounding without adding threads or changing defaults.
- Keep frame metadata consistent with color conversions: retain or restore HDR10/HDR10+ only for compatible output colorimetry and Dolby Vision only while its source color encoding remains compatible. Remove incompatible native side data, wrapper HDR/DOVI properties and attachments so encoding cannot restore them after tone mapping. Update replacement color descriptions and preserve SDR color signaling even when an encoder does not support HDR10.
- Use a valid BM3D block size, resolve Decimate's cadence automatically from stream metadata or a first pass over source timestamps and durations without caller-supplied FPS, propagate its negotiated output cadence, and initialize Yadif/Bwdif delayed frames as genuinely empty while emitting their final buffered picture at EOF.
- Correct FFmpeg frame-filter buffering and EOF handling:
	- Return a genuinely empty `AVFrame` on graph `EAGAIN` or EOF, preventing the endless zero-sample `adeclick` flush loop.
	- Consume inputs that a graph retains instead of passing them through, drain all available outputs, and emit delayed EOF frames using retained input lineage rather than silently dropping them.
	- Carry decoder/source/sink timebases through graph processing, preserve the input end timestamp when closing the graph, and update emitted timing and configured FPS while retaining unique output parts.
- Translate FFmpeg chroma positions to zimg's distinct enum values and include input range, matrix, transfer, primaries and chroma position in cached conversion keys.
- Strengthen filter tests with explicit output properties, bounded EOF/sample conservation, FPS frame counts and timestamps, zimg cache comparisons and SDR color tags; replace the invalid TTML/Matroska success expectation with TTML/MP4 coverage and an explicit Matroska rejection case.
- Peek compressed audio/video packet payloads during decoder submission without extracting bytes or advancing the FIFO cursor, so an `EAGAIN` retry resubmits the original data rather than an empty packet.
- Abort shared pipeline hoppers on failure or cancellation to wake capacity-blocked producers, cancel all facade stages before their teardown joins, and discard parked demux packets on failure rather than draining into stopped consumers. Keep normal EOF draining ordered; final demux worker destruction only joins feeders without accessing the owning stage's retired synchronization state. Cover missing decoders with compressed AAC input and consecutive VMAF remux teardown.
- Use default pthread attributes for bundled Kvazaar workers on MSVC instead of passing an uninitialized attribute object to thread creation.
- Replace thread-local scaling owners with reusable per-filter `AVFrame::ScaleContext` caches on all platforms. Graphs and Zimg workers remain reusable across frames and are released through ordinary filter teardown rather than DLL thread detach; direct callers can retain an explicit context through the new `ScaleTo` overload.
- Apply route input capacities after connecting their hoppers; configuring a missing hopper previously left first-filter or destination queues unbounded after audit wiring was reordered.
- Install route audit consumers before their producers, synchronize clone destination registration with emission and EOF, and propagate EOF to clone destinations connected after output closes.
- Retry video decoder EOF submission after draining pending frames when FFmpeg returns `EAGAIN`, rather than treating an unaccepted EOF as successfully signalled.
- Declare SVT-AV1 encoder support for HDR10 signaling so HDR10 AV1 jobs can select `libsvtav1`.
- Synchronize decoded side data across all bundled FFmpeg frame workers; system FFmpeg must be 9.0.2 or newer to avoid incomplete HEVC/Dolby Vision worker state.
- Drop remaining HEVC slices when the first slice of a picture is skipped, avoiding stale-context reference-list errors in frame-threaded decode.
- Preserve a remuxed video's average frame rate so Matroska retains the source `DefaultDuration` instead of estimating a misleading rate from probe packets.

[Unreleased]: https://github.com/StormByte-Suite/StormByte-Multimedia/compare/1.0.0...HEAD

## [1.0.0] - 2026-10-10

### Added

- `Property::Space::IPTC2` represents FFmpeg's IPT-PQ-C2 matrix coefficient, including Dolby Vision Profile 5 stream signaling.
- Initial public codec/container registry with names, aliases, extensions, available operations and container compatibility tables. `File` inspects streams, languages/titles, attachments, duration, audio layouts and video properties, including explicit HDR10 metadata and signaled HDR10-like heuristics.
- Owned `Plan` inputs/outputs and asynchronous worker stages (`Demuxer`, `Decoder`, `Remuxer`, `Encoder`, `Muxer`). The fluent `Transcoder` facade supports stream mapping, omitted-track dropping, quality settings, metadata overrides, attachment MIME selection, lifecycle hooks and pause/resume/cancel. `Transcoder::Track::Implementation(ImplementationSide, name)` independently selects decoder and encoder implementations while retaining the one-argument encoder shorthand.
- Generic libavformat muxing for registered single-file destinations, with separately maintained Matroska, WebM and MOV/MP4 policies selected from the actual writer filename. Remux preserves source language/title unless overridden; EOF feeders preserve per-track packet order, valid signed timestamps are retained, and destinations are truncated before replacement. Writing-app branding and Matroska-specific adaptations are preserved; formats requiring multiple outputs or their own file management remain outside the buffered-writer contract.
- Audio/video process filters, packet filters and two-pass processing, with independently owned analytics routes. VMAF compares decoded source and destination pictures per track and exposes mean/minimum scores with round-trip double precision and scored-frame counts. Experimental regional Degrain remains heuristic rather than motion-compensated restoration.
- Bitmap subtitle OCR through Tesseract, including language-aware PGS-to-SubRip conversion, and bundled or system OCR language models. Attachment inclusion is explicit; unselected attachments are omitted.
- Automatic supported-layout adaptation for audio encoding, including permitted 7.1-to-5.1 AC-3/E-AC3 conversion without a manual filter. MP3 inputs with more than two channels require an explicit downmix filter.
- Progress snapshots and retained stage/job telemetry for media counts, operation timing, waits and sampled process RSS. Shared owners, text, binary data and synchronization follow StormByte `Safe` lifetime contracts.
- Explicit failure contracts for invalid sources, unknown destinations, incompatible tracks/codecs, duplicate mappings, invalid MIME patterns and unsupported output shapes. Configuration failures remain terminal and encoder error details survive muxer propagation.
- Shared-library builds by default, static opt-in, BuildMaster dependency integration and Linux/macOS/Windows CI configurations. CMake reads the release version from `VERSION`.
- Encoder table entries for bundled Kvazaar and OpenH264, preserving x265/x264 as preferred implementations.

### Changed

- Mux compatibility is delegated to FFmpeg rather than maintained through a destination whitelist and artificial stream-shape restrictions. The actual output filename determines the muxer and exceptional format policy; incompatible codec/container combinations remain explicit failures.

### Fixed

- Do not classify Dolby Vision Profile 5 IPT-C2/PQ streams as HDR10 from BT.2020 primaries and PQ transfer alone; HDR10 heuristics also require a BT.2020 NCL/CL matrix.
- Mux stream reservation, packet metadata and short-source finalization:
	- Keep remux input indices separate from encoder output indices during reservation and packet routing, avoiding false duplicate connections and misdirected packets in selected or reordered multitrack outputs. Respect encoder reservation failures before publishing a connection.
	- Preserve original FFmpeg packet properties and side data when rebuilding mux packets, including late AV1 extradata from libaom and audio skip-sample metadata; copy properties after payload allocation so packet initialization does not erase them.
	- Allow remux codec parameters to be cloned after a short source reaches EOF, so subtitle-only outputs can write their headers instead of finishing with an empty file.
- Subtitle preparation and cue handling:
	- Prepare configured subtitle encoders during mux reservation so their real codec parameters and extradata are available before the first cue. Remove the flush-time substitution of unopened encoders with SubRip, preserving audio/video decoder errors and declaring delayed subtitle tracks without waiting for content.
	- Supply MOV timed-text encoders with ASS rectangles using FFmpeg's internal dialogue format while retaining cue timestamps and durations.
- Cover art and frame-filter construction:
	- Adapt JPEG, PNG and BMP attachments to MOV/MP4 cover-art packets instead of silently dropping image payloads. Retain Matroska attachment serialization and File's existing cover-classification workaround; explicitly reject resources that this MP4 backend cannot represent.
	- Implement the Watermark setup hook so path and binary overlays can be instantiated through the pipeline's filter API.
- Pipeline startup, connections and terminal failure propagation:
	- Prevent Demuxer setup from missing an immediately bound Plan.
	- Allow decoder initialization after a short source reaches EOF while its format context and queued packets remain available, avoiding endless setup waits.
	- Create planned per-track hopper connections before generic stage transfer, avoiding blocked producers and consumers when stages are wired before their first item. Reserve manual encoder mux streams through a separate typed `Encoder >> Muxer` connection.
	- Propagate stage failures observed as the muxer closes, even when the filter graph is already idle.
	- Honor explicit decoder names and manual Plan decoder pins when opening origin decoders. Missing or codec-mismatched implementations fail instead of silently using the default decoder.
	- Apply the calculated FFmpeg interleave limit to every output policy, so MP4 and generic muxers retain the packet-buffering margin previously configured by the shared muxer.
- Audio conversion and encoder submission:
	- Normalize unspecified PCM input layouts consistently with libswresample using a temporary frame reference, without modifying the original samples or channel count.
	- Normalize unspecified audio output layouts during sample conversion and drain; retry audio frame and EOF submission after a receive operation consumes buffered input without producing a packet.
- Encoder configuration and diagnostics:
	- Preserve HDR10 and Dolby Vision side data already carried by replacement frames; restore missing metadata only, retain recalculated HDR10+ instead of overwriting it, and avoid duplicate DOVI entries when encoding.
	- Apply video-encoder parallelism per implementation: use explicit positive hardware-thread counts for x264, OpenH264, libaom and libvpx; cap x265 frame threads at 15 while keeping its worker pool sized to the available CPUs; enable row-mt for libaom and VP9; retain SVT-AV1 and Kvazaar automatic parallelism.
	- Format decoder implementation labels, encoder-option errors and muxer error messages as text instead of character ranges.
	- Explicitly disable empty construction of the private opened-encoder result, which requires an initialized FFmpeg encoder.
- Bundled dependency interoperability:
	- Patch FFmpeg to read MOV/MP4 track `name` metadata as raw UTF-8, matching its writer and avoiding the spurious UDTA length-parsing fallback warning. System FFmpeg remains unchanged.
	- Disable IPO for all x265 bit-depth variants on every compiler to prevent cross-variant LTO from combining incompatible definitions of `x265_analysis_distortion_data`.
	- Keep x264 outside GCC LTO so its 64-byte stack preference does not suppress required stack realignment in other SIMD dependencies, avoiding AVX2 crashes in VMAF, VP9 encoding and OCR when GPL codecs are enabled.

### Tests

- 112 independently registered CTest cases in the validated bundled GPL/nonfree configuration, with per-case lifecycle output and bounded waits. Category-specific executables share compiled helpers to reduce recompilation while retaining individual CTest cases. Registration and skips depend on build options; the Japanese OCR case is registered only with bundled OCR.
- Coverage of codec, container and pipeline behavior:
	- Registry aliases, hardcoded `File` properties, facade/manual remux, attachment inclusion/omission, HDR encoding, Japanese OCR, audio conversions and negative input/configuration cases.
	- Seventeen audio and eleven video implementation cases, including bundled nonfree FDK-AAC, SVT/libaom AV1 encoding and dav1d decoding. Decoder roundtrips create their intermediate inputs within the same case.
	- Eighteen MP4 policy cases covering stream combinations, ordering and languages, audio filename aliases, mixed remux/encode, byte-exact image roundtrips in MP4/M4A/Matroska and unsupported font attachment rejection. Six mux-policy regressions cover generic destinations and incompatible outputs.
	- Three subtitle-header cases cover preparation without cues and a first cue timestamped at one hour. Empty-cue pipelines have a connected cue-consuming filter rather than decoder output without a consumer.
	- Facade/manual decoder-pin cases cover successful selection, missing implementations and codec mismatches, plus immediate Plan binding.
	- A dual-input watermark case uses the same synthetic colored-noise logo from a path and `#embed` bytes. Both VP9 outputs are inspected with `File` and their paths are printed for manual overlay inspection.
- VP9 and HEVC remux analytics require VMAF mean and minimum exactly 100 and all 48 fixture frames. Dedicated cases preserve automatic 7.1-to-5.1 AC-3/E-AC3 conversion and reject implicit 5.1-to-MP3 conversion.
- Tests use Multimedia APIs and `File` in process, without invoking external FFmpeg programs. Generated outputs are checked for codec/container identity, stream properties and duration; watermark appearance remains a manual check. Synthetic fixtures and redistributed font notices are documented separately. Optional codec cases skip only unavailable implementations or missing registry write support, not actual encoding failures; FDK-AAC is required in bundled nonfree builds.

[1.0.0]: https://github.com/StormByte-Suite/StormByte-Multimedia/releases/tag/1.0.0

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

### Fixed

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
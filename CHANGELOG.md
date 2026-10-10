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

### Added

- `Transcoder::Track::Implementation(ImplementationSide, name)` for independent decoder/encoder selection, retaining the one-argument encoder shorthand, and six facade/manual decoder-pin regression cases covering successful selection, missing implementations and codec mismatches.
- Encoder table entries for bundled Kvazaar and OpenH264, preserving x265/x264 as preferred implementations.
- Category-specific pipeline test executables with shared compiled helpers to reduce recompilation while preserving individual CTest cases.

### Fixed

- Implement the Watermark setup hook so path and binary overlays can be instantiated through the pipeline's filter API.
- Disable IPO for all bundled x265 bit-depth variants on every compiler to prevent cross-variant LTO from combining incompatible definitions of `x265_analysis_distortion_data`.
- Keep bundled x264 outside GCC LTO so its 64-byte stack preference does not suppress required stack realignment in other SIMD dependencies, avoiding AVX2 crashes in VMAF, VP9 encoding and OCR when GPL codecs are enabled.
- Prevent Demuxer setup from missing an immediately bound Plan.
- Propagate stage failures observed as the muxer closes, even when the filter graph is already idle.
- Create planned per-track hopper connections before generic stage transfer, avoiding blocked producers and consumers when stages are wired before their first item. Reserve manual encoder mux streams through a separate typed `Encoder >> Muxer` connection.
- Format decoder implementation labels as text rather than character ranges.
- Honor explicit decoder names and manual Plan decoder pins when opening origin decoders. Missing or codec-mismatched implementations fail instead of silently using the default decoder.
- Avoid unnecessary stream and attachment copies in read-only test range loops.
- Normalize unspecified PCM input layouts consistently with libswresample during sample-format conversion, using a temporary frame reference without modifying the original samples or channel count.

[Unreleased]: https://github.com/StormByte-Suite/StormByte-Multimedia/compare/1.0.0...HEAD

## [1.0.0] - 2026-10-08

### Added

- Initial public codec/container registry with names, aliases, extensions, available operations and container compatibility tables. `File` inspects streams, languages/titles, attachments, duration, audio layouts and video properties, including explicit HDR10 metadata and signaled HDR10-like heuristics.
- Owned `Plan` inputs/outputs and asynchronous worker stages (`Demuxer`, `Decoder`, `Remuxer`, `Encoder`, `Muxer`). The fluent `Transcoder` facade supports stream mapping, omitted-track dropping, codec/implementation selection, quality settings, metadata overrides, attachment MIME selection, lifecycle hooks and pause/resume/cancel.
- Matroska/WebM and MP4 muxing, plus audio-only MP3, Ogg, Opus, AC-3 and WAV output. Remux preserves source language/title unless overridden; EOF feeders preserve per-track packet order, valid signed timestamps are retained, and destinations are truncated before replacement.
- Audio/video process filters, packet filters and two-pass processing, with independently owned analytics routes. VMAF compares decoded source and destination pictures per track and exposes mean/minimum scores with round-trip double precision and scored-frame counts. Experimental regional Degrain remains heuristic rather than motion-compensated restoration.
- Bitmap subtitle OCR through Tesseract, including language-aware PGS-to-SubRip conversion, and bundled or system OCR language models. Attachment inclusion is explicit; unselected attachments are omitted.
- Automatic supported-layout adaptation for audio encoding, including permitted 7.1-to-5.1 AC-3/E-AC3 conversion without a manual filter. MP3 inputs with more than two channels require an explicit downmix filter.
- Progress snapshots and retained stage/job telemetry for media counts, operation timing, waits and sampled process RSS. Shared owners, text, binary data and synchronization follow StormByte `Safe` lifetime contracts.
- Explicit failure contracts for invalid sources, unknown destinations, incompatible tracks/codecs, duplicate mappings, invalid MIME patterns and unsupported output shapes. Configuration failures remain terminal and encoder error details survive muxer propagation.
- Shared-library builds by default, static opt-in, BuildMaster dependency integration and Linux/macOS/Windows CI configurations. CMake reads the release version from `VERSION`.

### Tests

- 49 independently registered CTest cases with 30-second timeouts and per-case lifecycle output. Coverage includes registry aliases, hardcoded `File` properties, facade/manual remux, attachment inclusion/omission, HDR encoding, Japanese OCR, audio conversions and negative input/configuration cases.
- VP9 and HEVC remux analytics require VMAF mean and minimum exactly 100 and all 48 fixture frames. Dedicated cases preserve automatic 7.1-to-5.1 AC-3/E-AC3 conversion and reject implicit 5.1-to-MP3 conversion.
- Tests use Multimedia APIs in process, without invoking external FFmpeg programs. Synthetic fixtures and redistributed font notices are documented separately; encoder cases skip only when the configured registry lacks write support.

[1.0.0]: https://github.com/StormByte-Suite/StormByte-Multimedia/releases/tag/1.0.0
# StormByte

Contributor guides: [Contributing](CONTRIBUTING.md) and [Coding Style](CODING_STYLE.md).

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![C++26](https://img.shields.io/badge/C%2B%2B-26-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.25+-064F8C?logo=cmake&logoColor=white)
![License: LGPL v3 / Commercial](https://img.shields.io/badge/License-LGPL_v3%20%2F%20Commercial-blue.svg)
[![CI](https://github.com/StormBytePP/StormByte-Multimedia/actions/workflows/ci.yml/badge.svg)](https://github.com/StormBytePP/StormByte-Multimedia/actions/workflows/ci.yml)
[![Sponsor](https://img.shields.io/badge/Sponsor-StormBytePP-ea4aaa?logo=githubsponsors)](https://github.com/sponsors/StormBytePP)

**StormByte-Multimedia** turns a media file into a checked, asynchronous C++ job. Map the streams you want, say which ones to copy or encode, call `Run()`, and observe progress or the final error. The fluent `Transcoder` handles the demux/decode/filter/encode/mux machinery; everyday applications work with `File`, registered codecs, and typed filters rather than FFmpeg's C API.

Start with `Transcoder` for a File-to-File job. When a product needs custom policy, derive from it and override plan factories or lifecycle hooks. When the graph itself is unusual, wire the same stages by hand. Custom filters use StormByte's RAII frame and packet types; reach for the libav-backed filter surface only when those wrappers do not provide the operation you need.

It depends on [StormByte Base](https://github.com/StormByte-Suite/StormByte/releases/tag/2.2.1) 2.2.1, [StormByte Buffer](https://github.com/StormByte-Suite/StormByte-Buffer/releases/tag/2.0.4) 2.0.4, [StormByte Logger](https://github.com/StormByte-Suite/StormByte-Logger/releases/tag/2.0.4) 2.0.4 and [StormByte System](https://github.com/StormByte-Suite/StormByte-System/releases/tag/2.1.1) 2.1.1. Each is a release archive. Public headers live under `StormByte/multimedia/` and cover the registry, containers, codecs, `File`, and the pipeline (`Plan`, `Step`, `Transcoder`, filters).

The suite is split on purpose. Base, Buffer, Config, Crypto, Database, Logger, Network and System are **other repositories**. This one does not implement them.

## What this module does

- **Build complete media jobs** — map input streams to output tracks, choose which are copied or re-encoded, set codec options, and select a destination. Output-track order determines mux order; omitted streams are dropped. `Plan::Check()` validates the job description, while actual codec and container support is checked when the job is configured and run.
- **Choose your level of control** — `Transcoder` gives applications a concise File-to-File API with asynchronous execution, progress, pause/resume/cancel, reports and lifecycle hooks. It is designed to be inherited: specialize its plan and hooks for a product workflow, batch runner or UI. For custom routing, connect `Plan`, `Demuxer`, `Decoder`, `Remuxer`, filters, `Encoder` and `Muxer` directly with `operator>>`.
- **Compose filters and analysis** — use typed audio/video filters on decoded media, packet filters where appropriate, and analytics such as VMAF. Filter chains work with either entry point. A recoverable filter condition follows that filter's contract; invalid configuration or processing failures remain visible in job status.
- **Discover available capabilities** — the registry reports the codecs, containers and operations available in the configured build. Use it to select implementations and check read/write access; availability depends on the chosen dependencies and build options, and a codec/container combination may still be rejected during job setup.
- **Integrate with application logging** — pipeline stages accept a shared StormByte Logger instance (prefer `ThreadedLog` when several workers can write). `Transcoder` exposes hooks and status so the application can own its UI, scheduling and error handling.

## The rest of the suite

| Module | Role | API |
| --- | --- | --- |
| [Base](https://github.com/StormBytePP/StormByte) | Exceptions, Expected, serialization, strings, UUID, concepts | [/StormByte](https://suite.stormbyte.org/StormByte) |
| [Buffer](https://github.com/StormBytePP/StormByte-Buffer) | FIFO, SharedFIFO, Ring, Producer/Consumer and multi-stage pipelines | [/StormByte-Buffer](https://suite.stormbyte.org/StormByte-Buffer) |
| [Config](https://github.com/StormBytePP/StormByte-Config) | Human-readable text and versioned binary documents (groups, lists, raw bytes) | [/StormByte-Config](https://suite.stormbyte.org/StormByte-Config) |
| [Crypto](https://github.com/StormBytePP/StormByte-Crypto) | Hash, compress, encrypt, sign and key agreement — Crypto++ never leaves the private tree | [/StormByte-Crypto](https://suite.stormbyte.org/StormByte-Crypto) |
| [Database](https://github.com/StormBytePP/StormByte-Database) | One API over SQLite, PostgreSQL and MariaDB | [/StormByte-Database](https://suite.stormbyte.org/StormByte-Database) |
| [Logger](https://github.com/StormBytePP/StormByte-Logger) | Stream logging, levels, headers, redaction, `ThreadedLog` | [/StormByte-Logger](https://suite.stormbyte.org/StormByte-Logger) |
| **Multimedia** | This repository | [/StormByte-Multimedia](https://suite.stormbyte.org/StormByte-Multimedia) |
| [Network](https://github.com/StormBytePP/StormByte-Network) | Framed packets, Client/Server, IPv4/IPv6 TCP and Buffer pipelines (compress/encrypt) | [/StormByte-Network](https://suite.stormbyte.org/StormByte-Network) |
| [System](https://github.com/StormBytePP/StormByte-System) | Processes, pipes and environment variables across Linux, Windows and macOS | [/StormByte-System](https://suite.stormbyte.org/StormByte-System) |

## Table of Contents

- [Quick start](#quick-start)
- [What this module does](#what-this-module-does)
- [The rest of the suite](#the-rest-of-the-suite)
- [Documentation](#documentation)
- [Codec compatibility](#codec-compatibility)
- [Two ways to work](#two-ways-to-work)
  - [1. Transcoder (File → File)](#1-transcoder-file--file)
  - [2. The tube by hand](#2-the-tube-by-hand)
- [Plan, items and the tube contract](#plan-items-and-the-tube-contract)
- [DLL boundaries](#dll-boundaries)
- [Filters and analytics](#filters-and-analytics)
- [Progress](#progress)
- [Logging](#logging)
- [Build options and distribution](#build-options-and-distribution)
- [Installation](#installation)
- [Tests](#tests)
- [Contributing](#contributing)
- [License](#license)
- [Supporting the project](#supporting-the-project)

## Documentation

- This README: how to build, the two entry points, the tube contract, codec compatibility and distribution flags.
- Doxygen class reference (headers under `StormByte/multimedia/`): [https://suite.stormbyte.org/StormByte-Multimedia/](https://suite.stormbyte.org/StormByte-Multimedia/).
- Logger contract used by every `Step`: [https://suite.stormbyte.org/StormByte-Logger/](https://suite.stormbyte.org/StormByte-Logger/).

## Quick start

Remux the first video stream from one file into Matroska. `Transcoder` infers the destination container from the output suffix; the application does not create FFmpeg contexts or connect pipeline stages.

```cpp
#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

#include <chrono>
#include <iostream>
#include <thread>

using StormByte::Logger::Level;
using StormByte::Logger::ThreadedLog;
using StormByte::Multimedia::Pipeline::Status;
using StormByte::Multimedia::Pipeline::Transcoder;
using StormByte::Safe::String;

int main(int argc, char** argv) {
	if (argc != 3) {
		std::cerr << "usage: " << argv[0] << " <input> <output.mkv>\n";
		return 1;
	}

	auto logger = StormByte::Safe::MakeShared<ThreadedLog>(std::clog, Level::Notice, "[%L] %T %c");
	Transcoder job{String{argv[1]}, String{argv[2]}, logger};
	job.Video(0).Remux();
	if (job.Failed())
		return 1;

	job.Run();
	for (;;) {
		const auto status = job.Status();
		if (status == Status::Done)
			return 0;
		if (status == Status::Error || status == Status::Aborted) {
			std::cerr << job.Error().value_or(String{"media job failed"}) << '\n';
			return 1;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}
```

Track mapping is the whole job description: call `Video(n)`, `Audio(n)` or `Subtitle(n)`, then choose `Remux()` or `Codec(...)`. Add only the streams you want in the output. The [full example below](#1-transcoder-file--file) adds encoding, filters, attachments and progress reporting.

## Codec compatibility

`Codec::SubtitleKind()` returns `Safe::Optional<SubtitleKind>`: empty for codecs
that are not subtitles, otherwise `Text`, `Bitmap` or `Unknown`. Classification
uses FFmpeg codec descriptors, independently of encoder/decoder availability.
SRT, ASS, WebVTT and binary-packed `mov_text` are text; PGS, DVD/DVB subtitles
and XSUB are bitmap. `Unknown` means the subtitle descriptors do not provide an
unambiguous classification. This describes subtitle content, not whether the
file contains binary bytes, what a particular decoder emits, or whether OCR is
available. It is accessible through registry identities and `stream.Codec()`.

`Codec::HasAccess(Access)` remains a generic capability query. Use the
`HasAccess(Access, Features)` overload to require all features from one available
implementation in each requested direction:

```cpp
#include <StormByte/multimedia/registry.hxx>

using namespace StormByte::Multimedia;

auto hevc = Registry::Instance().FindCodec("H.265");
if (hevc) {
	const Codec& codec = hevc->get(); // CodecRef borrows the registry identity.
	const bool writable = codec.HasAccess(Operation::Write);
	const bool hdrWritable = codec.HasAccess(Operation::Write, Feature::HDR10);
	const bool doviReadable = codec.HasAccess(Operation::Read, Feature::DOVI);
	const bool hdrRoundTrip = codec.HasAccess(
		Operation::Read | Operation::Write, Feature::HDR10 | Feature::TenBit);
}
```

An empty feature mask is exactly the generic query, including for codecs absent
from the implementation tables. With nonempty features, matching uses the
StormByte identity (`H.265`, `E-AC3`), the row's full feature mask, and availability
of its specific FFmpeg implementation name. Registry aliases such as `hevc` and
`eac3` resolve to the same identities before querying. Encoder matching shares
the pipeline selector and its lowest-preference rule: a build with only
`libkvazaar` can write H.265 but cannot write it with HDR10 or Dolby Vision.
Decoder matching uses the decoder tables; a direction without a covering,
available row returns false.

This is a compiled-implementation check, not an attempt to open a codec or
hardware device. It does not guarantee valid settings, device availability,
container compatibility, or preservation of metadata after filters. It does not
change decoder selection, encode rules or plan policies.

`Feature::DOVI` indicates that a listed implementation supports Dolby Vision metadata handling for that direction. It is separate from HDR10, HDR10+ and hardware-acceleration support; it does not promise support for every Dolby Vision profile or certify the visual result.

| Direction | Format | Implementations listed with DOVI support |
| --- | --- | --- |
| Decode | HEVC | `hevc` |
| Decode | AV1 | `av1`, `libdav1d` |
| Encode | HEVC | `libx265` (requires a GPL-enabled build and a compatible x265) |
| Encode | AV1 | `libsvtav1`, `libaom-av1` (availability depends on the configured dependencies) |

The registry reflects the capabilities compiled into the selected bundled or system libav build. Decoding remains available independently of GPL encoder components. Encoding requires compatible Dolby Vision input metadata and supported encoder settings; an encoder cannot create a Dolby Vision grade from ordinary HDR pixels alone. Other hardware implementations may decode or encode the picture format without providing the metadata support represented by `Feature::DOVI`.

### HDR10+ metadata

`Feature::HDR10Plus` describes SMPTE ST 2094-40 dynamic metadata handling, separately from static HDR10. Native HEVC (`hevc`) reads registered ITU-T T.35 SEI; native AV1 (`av1`) and `libdav1d` read ITU-T T.35 metadata OBUs. The `libx265` encoder consumes native frame HDR10+ side data and writes registered HEVC SEI; `libaom-av1` consumes it through FFmpeg's `add_hdr_plus` and writes AV1 metadata OBUs. These declarations depend on the selected FFmpeg and codec-library build, not just support for ten-bit pixels. No HDR10+ capability is inferred for other software or hardware implementations.

VP9 is container-conditional: `libvpx-vp9` encoding carries HDR10+ as packet side data, which Matroska serializes as ITU-T T.35 BlockAdditions. Matroska restores that packet metadata, and FFmpeg's generic decoder path transfers it to native frames for `vp9` and `libvpx-vp9`. This is not a VP9 bitstream HDR10+ feature and does not promise preservation in raw VP9 or arbitrary containers. The registry feature alone is not a container compatibility guarantee.

Implementation references: FFmpeg's [libaom encoder](https://ffmpeg.org/doxygen/trunk/libaomenc_8c_source.html), [x265 encoder](https://ffmpeg.org/doxygen/trunk/libx265_8c_source.html), [native HEVC SEI parser](https://ffmpeg.org/doxygen/trunk/hevc_2sei_8c_source.html), [native AV1 decoder](https://ffmpeg.org/doxygen/trunk/av1dec_8c_source.html), [dav1d wrapper](https://ffmpeg.org/doxygen/trunk/libdav1d_8c_source.html), [libvpx encoder](https://ffmpeg.org/doxygen/trunk/libvpxenc_8c_source.html), [generic decoder](https://ffmpeg.org/doxygen/trunk/decode_8c_source.html), and [Matroska muxer](https://ffmpeg.org/doxygen/trunk/matroskaenc_8c_source.html) / [demuxer](https://ffmpeg.org/doxygen/trunk/matroskadec_8c_source.html).

The fixed [HDR10+ fixture](test/files/README.md#hdr10) has 48 frames with alternating metadata. In-process tests compare normalized T.35 payloads on every decoded frame, timestamps, frame counts, static HDR10 and color properties through remux, x265, libaom and Matroska VP9. The HEVC -> AV1 -> HEVC test also extracts the final elementary HEVC stream through `Transcoder` and decodes it with native HEVC: container side data cannot satisfy that check. Static HEVC/AV1 comparisons allow only the existing AV1 representation quantization tolerance. Tests never invoke external media tools or regenerate fixtures.

### Dolby Vision metadata and transforms

Dolby Vision and HDR10 are separate properties: DOVI-only content must not acquire an invented HDR10 fallback. Preserving metadata does not prove that it still describes transformed pixels. Cropping, scaling, overlays, denoising, grading, gamut conversion and tone mapping can make it stale. Validate or regenerate it with an appropriate mastering workflow after such transformations. Neither a successful encode nor metadata preservation proves visual correctness, Dolby conformance or certification.

## Two ways to work

You either let `Transcoder` assemble a job from a fluent map of origin streams, or construct the `Step`s yourself and join them with `operator>>`.

### 1. Transcoder (File → File)

`Transcoder` is the recommended starting point for a File-to-File job. Construct it with source and destination paths, map **output** tracks in mux order, choose remux or encoding, attach filters, and run. The destination container is inferred from the writer path unless an explicit registered `const Container&` override is supplied. The stock class is complete for remuxing, transcoding and filtering; inheritance is for adding application-specific behavior, not a prerequisite for ordinary use.

To integrate with a UI, scheduler or batch runner, override lifecycle hooks such as `OnProgress`, `OnDone` and `OnError`. For application-specific state, specialize `EmptyPlan()` / `EmptySettled()` while retaining the same pipeline stages and behavior.

Open the source, map streams, run, poll:

```cpp
#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/multimedia/pipeline/filters/video/scale.hxx>
#include <StormByte/multimedia/pipeline/filters/video/watermark.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

using StormByte::Logger::Level;
using StormByte::Logger::ThreadedLog;
using StormByte::Multimedia::Registry;
using StormByte::Multimedia::Pipeline::Status;
using StormByte::Multimedia::Pipeline::Transcoder;
using StormByte::Multimedia::Pipeline::Filter::Video::Anchor;
using StormByte::Multimedia::Pipeline::Filter::Video::Scale;
using StormByte::Multimedia::Pipeline::Filter::Video::Watermark;
using StormByte::Safe::String;

int main(int argc, char** argv) {
	if (argc != 3) {
		std::cerr << "usage: " << argv[0] << " <in.mkv> <out.mkv>\n";
		return 1;
	}

	auto logger = StormByte::Safe::MakeShared<ThreadedLog>(std::cout, Level::Debug, "[%L] %T %c");
	Transcoder job{String{argv[1]}, String{argv[2]}, logger};

	auto& registry = Registry::Instance();
	auto hevc = registry.FindCodec("H.265");
	auto eac3 = registry.FindCodec("E-AC3");
	if (!hevc || !eac3) {
		std::cerr << "codec missing in this build\n";
		return 1;
	}

	// Output order is the order of these calls. Origin index is the argument.
	job.Video(0)
		.Codec(hevc->get())
		.Implementation(String{"libx265"})
		.Filter<Watermark>(logger, String{"/var/lib/marks/logo.png"},
			Anchor::BottomRight, 25)
		.Filter<Scale>(logger, 0u, 1080u);
	job.Audio(1).Remux();          // compressed copy, adapted to the destination
	job.Audio(1).Codec(eac3->get()); // second output from the same origin
	job.Subtitle(3).Remux();
	job.Attachments();             // keep attachments; omit this call to drop them

	if (job.Failed()) {
		std::cerr << job.Error().value_or(String{"configure failed"}) << '\n';
		return 1;
	}

	job.Run();                     // non-blocking
	for (;;) {
		const auto status = job.Status();
		if (auto progress = job.Progress())
			std::cout << '\r' << static_cast<String>(*progress) << std::flush;
		if (status == Status::Done)
			break;
		if (status == Status::Error || status == Status::Aborted) {
			std::cerr << '\n' << job.Error().value_or(String{"job ended"}) << '\n';
			return 1;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}
	std::cout << "\ndone\n";
	return 0;
}
```

What that mapping means:

| Call | Effect |
| --- | --- |
| `Video(0).Codec(hevc->get()).Implementation(String{"libx265"})` | Decode origin video 0, encode HEVC with that encoder pin. |
| `.Filter<Watermark>(…)` / `.Filter<Scale>(…)` | Frame filters on that encode lane, in registration order. |
| `Audio(1).Remux()` | Keep the compressed stream. Remux is copy **plus** destination adaptation. There is no separate “Copy” stage. |
| `Audio(1).Codec(eac3->get())` after `Audio(1).Remux()` | Add an E-AC3 encode of the same origin alongside the compressed original. |
| `.Implementation(ImplementationSide::Decoder, String{"libdav1d"})` | Pin the source decoder on a recode track; unavailable or codec-mismatched names fail without fallback. |
| `Ignore(n)` | Drop every mapped output of origin stream `n`. |
| `Attachments()` / `Attachments("image/png")` | Keep all attachments, or only a MIME. Default without a call is drop. |
| `Transcoder(source, destination, logger)` | Supplies locations before mapping; the writer path determines the container. |
| `Transcoder(source, destination, container, logger)` | Uses the borrowed registered container identity regardless of the writer suffix. |
| `Filter<Analytics>(…)` on the **job** | Global analytics on matching encode or remux lanes; `Track::Filter` also accepts track-scoped analytics. |

`Run()` is asynchronous. `Pause()` / `Resume()` / `Cancel()` talk to the coordinator. After `Done`, `Reports()` holds analytics snapshots (VMAF mean/min and anything else you attached). Mux close is not analytics EOF: `Transcoder` waits for the route to go idle before `OnDone` / `Reports`.

Each `Video(n)`, `Audio(n)` or `Subtitle(n)` call appends a new output, not a lookup
of an existing mapping. Repeating an origin supports remux plus encode, several
encodes with different settings, or several remux copies. Each output has its own
codec settings, decoder pin, filters and metadata overrides; source metadata is
inherited independently. Save the returned `Track` handle to keep configuring that
output. Output order follows mapping order, and `OnSettled` identifies each encode
by its distinct `Out` slot even when several rows share the same `In`.
Analytics pair reference and destination frames by output slot, so per-track and
global reports keep duplicated outputs separate.

For example, with a DTS-HD MA source at stream 1 and a compatible destination such
as Matroska, the two audio calls above preserve the DTS-HD MA bitstream and add an
E-AC3 compatibility track. The configured build still needs a decoder for the source
and an E-AC3 encoder. Container restrictions apply to every output.

`job.Progress()` reports the duration scan, the main job percent and the optional measure and analytics phases. See [Progress](#progress) for how to read the real transcode percent in a UI.

Capture `job.Telemetry()` before `Run()` if the final snapshot must outlive the job. It retains each stage's origin, lifecycle, frame/packet input and output counts, setup and elapsed time, Process-call total/mean/min/max, and blocked-wait total/count/max. The coordinator also logs the final multi-line report at `Info`; converting the retained handle to `std::string` produces the same report. A hand-built `Step` or filter exposes its own retained `Telemetry()` handle.

`JobTelemetry::PeakMemory()` (also `MemoryMaximum()`) is the highest sampled process resident set size in bytes; `MemoryMinimum()` and `MemoryCurrent()` report the lowest sample and last sample. The coordinator samples nominally every 20 ms and at job start/end. These values are process-wide RSS, not memory attributed to an individual stage; OS sampling may miss short-lived peaks and can be unavailable on unsupported platforms.

Quality knobs on a recode track are the obvious ones: `CRF`, `BitRate`, `MaxBitRate`, `Preset`, `Tune`, `FineTune`, plus `Language` / `Title` overrides. A knob the track type or the selected encoder cannot apply fails the job instead of being ignored. `CRF` is a `double`; encoders whose quality option is an integer reject fractional values. On audio, `CRF` selects quality-based VBR (Vorbis, LAME MP3) and replaces `BitRate`; without either, the encoder's own default applies, never the source bitrate. `FineTune` takes individual keys: for encoders with a packed option (`x265-params`, `x264-params`, `svtav1-params`, ...) they are packed into it, and unknown keys fail when the encoder opens.

`Codec::Encoders(features)` lists the available encoder implementations in selection order. Each `EncoderInfo` tells which knobs it supports and through which FFmpeg option, and `Option(name)` describes that option's type, range, default and choices, so a UI can offer only settings that will be applied.

`Implementation(ImplementationSide::Encoder, String{"libsvtav1"})` selects the encoder; `Implementation(ImplementationSide::Decoder, String{"libdav1d"})` independently selects the decoder. The one-argument `Implementation(name)` remains shorthand for the encoder. An empty name restores default selection for that side. Pins must match the destination and source codecs respectively. A manual pipeline can set `Config::Implementation::Decoder` on the Plan track or call `Decoder::Implementation` before wiring; an explicit stage pin takes precedence over the Plan pin. Remux does not open an origin decoder and ignores these pins.

Video encoding negotiates input only after selecting the concrete encoder with the required features. A supported CPU format keeps its pixel buffers without conversion or a pixel copy, so x265's supported `yuv420p10le` input and encoder-selection defaults are unchanged. If necessary, the encoder can exactly repack `yuv420p10le` and `p010le` in either direction, using explicit little-endian byte access rather than a scaler. This changes storage layout only: component samples, 10-bit depth, 4:2:0 subsampling, color tags/range, geometry, timing and native or wrapper HDR/Dolby Vision metadata are preserved. Encoder preparation leaves shared source frames untouched and retains the same prepared frame across backpressure retries. Other unsupported representations fail explicitly; depth reduction, chroma resampling and color conversion require an explicit filter.

For encoders advertising hardware frames contexts, input preparation can create a device and reusable pool, upload exact software samples, reuse compatible source pools, or download hardware frames before an equivalent CPU submission. Device constraints and transfer formats must preserve the software representation; device or profile failures remain explicit rather than triggering a lower-quality or software-encoder fallback. This prepares encoder input and does not automatically activate hardware decoding. Implementation features describe potential support, not the capabilities of every installed device or driver. For example, `nvidia-vaapi-driver` exposes NVDEC decoding only; NVIDIA encoding uses NVENC, not that driver's VAAPI interface.

Audio layout adaptation permits automatic 7.1-to-5.1 encoding to AC-3/E-AC3 without a manual downmix filter. MP3 encoding rejects input with more than two channels unless an explicit downmix filter reduces it first. Configuration errors remain terminal: calling `Run()` on a failed job preserves its original error instead of starting processing.

### 2. The tube by hand

Same workers, no facade. You own construction, binding and lifetime. This is what you want for a custom graph (several destinations, an extra sink, a filter that is not on `Transcoder`’s fluent map — as long as it is still a `Step` / `Filter` the tube already understands).

A short recode of one video track into Matroska:

```cpp
#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/multimedia/pipeline/config/video.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/registry.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Logger::ThreadedLog;
using StormByte::Multimedia::Registry;
using namespace StormByte::Multimedia::Pipeline;

int main() {
	auto logger = StormByte::Safe::MakeShared<ThreadedLog>(std::cout, Level::Notice, "[%L] %T %c");
	auto hevc = Registry::Instance().FindCodec("H.265");
	if (!hevc || !hevc->get().HasAccess(StormByte::Multimedia::Access{
			StormByte::Multimedia::Operation::Write}))
		return 1;

	auto plan = StormByte::Safe::MakeShared<Plan>(
		StormByte::Safe::String{"in.mkv"}, StormByte::Safe::String{"out.mkv"});
	Config::Video video;
	video.Codec(hevc->get());
	plan->add(Track{0, std::move(video)});
	if (auto check = plan->Check(); !check) {
		std::cerr << check.error()->what() << '\n';
		return 1;
	}

	Demuxer demux(logger);
	Decoder decode(logger, 0);
	Encoder encode(logger, 0, hevc->get());
	Muxer mux(logger);
	const std::array<Step*, 4> stages{&demux, &decode, &encode, &mux};
	struct StopPeers {
		const std::array<Step*, 4>& stages;
		~StopPeers() {
			for (Step* stage : stages)
				stage->Stop();
		}
	} teardown{stages};
	demux >> mux;
	demux >> decode;
	decode >> encode;
	encode >> mux;
	plan >> demux;
	if (plan->State() == Plan::State::Failed)
		return 1;

	const auto stopped = [&] {
		return std::all_of(stages.begin(), stages.end(), [](const Step* stage) {
			return stage->Status() == State::Stopped;
		});
	};
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
	while (!stopped()) {
		if (plan->State() == Plan::State::Failed
			|| std::chrono::steady_clock::now() >= deadline)
			return 1;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return plan->State() == Plan::State::Finished ? 0 : 1;
}
```

`Plan` is noncopyable and nonmovable. Construct a `Safe::Shared<Plan>` and bind that handle with `plan >> demux`; there is no value binding or `Plan::Move()`. Stage constructors still launch their workers immediately, but setup waits for the shared Plan to become `Running`. Finish all configuration and typed connections before binding the Plan. Binding publishes `Running`. `Demuxer` produces `Packet`s and receives nothing. `Decoder` turns those into `Frame`s. `Encoder` produces `Packet`s again. `Muxer` reserves the output slot, not the remuxer. Fan-out from one demuxer to several decoders or remuxers uses the same operator.

Keep every stage alive through execution, failure propagation and Plan notifications. Request Stop on all peers before destroying any of them; stage destruction stops and joins its own worker. `Stop()` alone is nonblocking and a terminal status is not an all-worker join barrier. Externally serialize Plan state publication and teardown: no thread may publish a state or notify a borrowed stage while that stage is being destroyed. The example requests cancellation on all peers on every exit; applications with concurrent controllers must additionally quiesce those controllers before leaving the stage scope.

The library publishes `Finished` when every participating stage has completed successfully. Muxer completion alone is insufficient while filters or analytics are still active; keep the graph and its endpoints alive until `Idle`. On failure, preserve `Plan::Error()` and tear down the whole graph. Applications observe `plan->State()`; they do not publish it.

`Filters` sits between two `StormByte::Safe::Shared<Step>` endpoints when you need a filter chain or analytics; `Transcoder` builds that graph for you. In a manual graph, create the shared decoder and encoder, connect the demuxer and muxer, register filters and analytics, call `Filters::Close()`, and only then bind the Plan to the demuxer. This ordering ensures consumers are connected before workers start. Keep the graph and endpoints alive through completion and notifications, wait for `graph.Idle()` as well as every stage, and observe `Finished` rather than publishing it.

## Plan, items and the tube contract

- **`Plan`** describes the job: owned input/output locations, a private source-file snapshot and the **output** tracks. Use public configuration operations, including `add`, before binding; `Check()` and `Tracks()` are public. `Snapshot()`, `Container()`, `Reader()`, `Writer()` and `Path()` are private implementation accessors, not an application inspection surface. Inspect source media through `File` and retain `InputTelemetry()` / `OutputTelemetry()` for I/O counters.
- **Plan lifecycle** uses public `enum class Plan::State` with `Created`, `Running`, `Finished` and `Failed`. Applications only read `plan->State()`. Binding the shared plan to the demuxer publishes `Running`; the first formation or execution failure publishes `Failed` and is preserved; every participating stage completing successfully publishes `Finished`. Failed cannot restart, and Finished does not acquire a later error. `Check()` failure also publishes Failed. There is no public state setter. State, Error and telemetry are the public observation APIs; configure the intention before binding.
- **Policies** — `Plan::Policy` is a bitmask (`Plan::Policies`), same shape as `Feature` / `Features`. `None` is zero. `EnforceFeatures` (`1 << 0`) is set when the plan is constructed. `Policies()` returns the mask by const reference. `Policies(Policy)` adds that flag and does not replace the mask. `Policies(~Policy::EnforceFeatures)` clears only that flag. `Check()` always validates formation. While `EnforceFeatures` is set it also rejects an encode whose destination codec has no table row that keeps source HDR10, HDR10+ or Dolby Vision. With the flag off, that encode is allowed and a destination implementation that lacks the feature does not write it.
- **Container override**: `Plan(source, destination, const Container&, duration)` and the corresponding owned-reader/writer overloads override filename inference, even for an unknown or conflicting suffix. Supply a writable identity from `Registry::FindContainer`; the Plan borrows it rather than copying it. The registry entry must remain valid through Plan destruction. Without an override, the writer path still selects the registered buffered muxer. `Transcoder` also accepts the identity before its logger argument.
- **`Packet`** is a compressed access unit. **`Frame`** is a decoded one. No public timing setters. Mutate pixels through `Decoder` / `Encoder` / a filter `Replace`, not a setter on `Frame`.
- **`Serial`** is a monotone id assigned by the tube. Public getter, no setter. It is not a frame count.
- **`Remuxer`** forwards compressed packets and adapts them to the destination. “Copy” as a stage does not exist.
- **Interleaving** — encoders and filters add latency that differs per track (a video encoder can hold seconds of lookahead while audio flows at once), so the muxer never trusts arrival order. It holds packets and writes the oldest only once every audio and video track has one queued, so the file is in timeline order and a player that seeks to a video cue finds audio for the same instant next to it. Subtitles are sparse and never hold the others back. A track that stays silent for more than 60 seconds of timeline (ended or never started) stops holding the rest, which bounds memory. The `interleave` test label checks this on encoded, remuxed and two-pass audio jobs.
- **Caps** (hopper capacity and stage ceilings) limit individual queues, not total job memory. Do not treat EOF as Fail. Recoverable logo failures can become Watermark passthrough; invalid plans, unsupported encoders and fatal processing errors remain failures.
- **Frames and metadata** — filters can replace decoded content while the pipeline preserves the timing and properties it can safely retain. HDR and Dolby Vision metadata is not generally recalibrated to describe changed pixels; see [Dolby Vision metadata and transforms](#dolby-vision-metadata-and-transforms).
- **Music tags** — the muxer copies the source snapshot's title, artist, album, album artist, composer, genre, comment, copyright, date, track and disc numbers during encode and remux. The destination container decides how to store them: ID3 in MP3, Vorbis comments in Ogg/Opus/FLAC, or MP4 metadata in M4A. `File::Metadata()` reads these fields, preferring container values and falling back to stream comments only for a single audio stream without other media streams (covers and attachments do not count). Output encoder identification is generated afresh, not copied. This preserves the existing typed fields, not arbitrary ID3 frames, track/disc totals or a byte-identical ID3 block; formats without matching tags cannot preserve them. There is no public metadata editing API.
- **Output branding** — all muxed encode and remux jobs set editable container `encoder` / `encoding_tool` and media-stream `encoder` fields to `StormByte-Multimedia <version>` before writing the header. Formats serialize the fields they support; raw streams and formats without application tags cannot advertise them. Backend-defined technical identifiers remain truthful: for example Matroska's MuxingApp and Ogg/FLAC vendor strings can still identify libavformat, while the editable application/encoder fields identify StormByte. MP3 uses the container ID3 encoder tag for branding; its stream encoder field is left to FFmpeg because it also controls the Xing/LAME identifier needed to read gapless delay and padding correctly. Codec bitstream identifiers are not rewritten, and remux does not alter compressed payloads.

## DLL boundaries

The public API uses `StormByte::Safe` values (`String`, `Optional`), collections (`Vector`, `Map`, `Pair`) and owners (`Shared`, `Unique`) for owning data that crosses module boundaries. Owners retain provider-local release operations so the exact payload is destroyed by its creating provider. Construct shared owners with `Safe::MakeShared<T>(…)`; parameters typed as `Safe::String` require explicit `Safe::String{"text"}` construction. Fields holding references to external registry entries are borrowed, not owned: those registries must remain valid for the lifetime of the references.

`STORMBYTE_DECLARE_MAYBE_SAFE` is a provider's responsibility and promise, not an automatic audit: a declared type must keep its members, special members and heap-affecting operations boundary-safe. Consumers and providers still need a compatible C++/STL ABI. All providers supplying live values, owners or callbacks must remain loaded until those objects are released; this is neither arbitrary-ABI compatibility nor a safe-unload guarantee.

For custom payloads, derived `Transcoder` providers must override `EmptyPlan()` / `EmptySettled()` in their own module and create the exact derived payload with `Safe::Shared<Plan>::MakePointer<DerivedPlan>(…)` / `Safe::Unique<TrackSettled>::MakePointer<DerivedSettled>(…)`. Derived Plans retain shared ownership and do not implement `Clone()` or `Move()`; derived `TrackSettled` types must override both there. Preserve the dynamic type and its provider-local release operations rather than slicing to the base.

## Filters and analytics

Container output is selected from the destination path unless an explicit registered identity overrides it, and follows the format's supported stream and attachment rules. Matroska, WebM and MP4 support the common media workflows; image attachments can be written as cover art in MP4, while unsupported attachment types are reported rather than silently discarded. The library writes its application identity into supported container metadata. Formats that require multiple output files are outside the single-writer workflow.

Filters are leaves, not a second pipeline language. `Scale` is resize (that is the name). `Watermark` is a still image on decoded video, with Hold so a black slate at the start does not pin the letterbox probe too early.

Filters retain reusable scaling graphs and workers in their own provider-owned cache on every platform. For repeated direct frame scaling outside a filter, retain an `FFmpeg::AVFrame::ScaleContext` and call `source.ScaleTo(context, destination, width, height, filter, scaler)`. Use a context from only one thread at a time and destroy it during ordinary application teardown, not from a thread-local destructor or DLL detach callback. The original `ScaleTo` overload remains available for one-off calls but does not retain a cache across calls.

Custom frame filters use `Save` for a one-for-one replacement that retains the input's wrapper timing. Buffering or rate-changing filters use `Drop` when input is accepted without output, `EmitGraph` to emit a graph's first result and drain its available outputs, and `EmitFrame` for individually prepared outputs, including those produced during `Eof`. Emitted frames carry the output's native timebase and timestamps; their lineage comes from the current input, or the last input at EOF, with distinct parts for multiple outputs. Analytics cannot use these mutation methods. For direct graph use, set `AVFrame::TimeBase` consistently with its PTS and duration, call `Receive` to drain without closing, and stop `Flush` when it returns an empty frame.

Replacing a video frame also updates its color description. HDR10/HDR10+ metadata is retained only for compatible HDR10 output; Dolby Vision preservation additionally requires the source color encoding to remain unchanged. Tone mapping to SDR removes incompatible native metadata, wrapper properties and attachments so a later encoder cannot restore the original HDR tags. Encoders retain valid SDR color signaling independently of HDR support. Decimate resolves its cadence automatically from stream metadata, or from a first pass over timestamps and durations when metadata is missing; no caller-supplied FPS is required. Missing or irregular timing that cannot describe a constant cadence fails explicitly. The lower-level graph adapter also retains nominal-frame-rate `Open`/`Ensure` overloads for direct graph callers.

Pixel-only replacements inherit unspecified color tags from their input before metadata compatibility is checked; explicit replacement tags take precedence. NlMeans reuses rolling patch-distance sums with the same search radii, strength and border handling, without adding worker threads or changing its default profile.

Filter tests distinguish observable transformations from restoration quality. Channel count and sample rate, crop/scale/pad dimensions, output cadence, pending-frame counts and EOF termination have explicit assertions. Denoising and sharpening smoke tests check processing and valid output, not a universal perceptual improvement on arbitrary noise. The zimg regression compares cached conversions with fresh contexts as input color tags change; the HDR-to-SDR test also checks the encoded BT.709 color description.

`Degrain` is an experimental two-pass regional film-grain reducer. Attach it with `.Filter<Degrain>(logger)` or `.Filter<Degrain>(logger, sigmaCap)` before sharpening or scaling. Measurement uses a 12 x 8 grid and up to five neighbouring pictures on each side, compensates local brightness drift, and conservatively suppresses filtering for motion, texture and skin-like colours. Darkness alone never forces filtering. Clean or inconclusive frames, missing/duplicate measurement timestamps, and unsupported pixel layouts pass through unchanged. The default sigma ceiling is 4.0 in 8-bit-equivalent units; zero disables filtering, finite ceilings are clamped to [0, 100], and non-finite ceilings fail the filter.

Degrain blends the original and up to three bracketing filtered strengths using spatially smooth regional targets. It supports software planar integer YUV/gray at 8/10/12/16 bits in either byte order, preserving geometry, format, original PTS, metadata and alpha. Denoising uses the previous real input and current input, with current duplicated in place of a future picture; detected discontinuities use current alone. This asymmetric temporal baseline produces one output per input without delay, but is not motion compensation. Brightness cuts/flashes isolate temporal smoothing; equal-brightness cuts, fine texture and correlated/compressed grain remain heuristic limitations. Measurement holds at most eleven pictures plus compact per-frame target maps; repeated filtering is expensive. Do not stack it with another grain-denoising filter.

Analytics do not change the media output. VMAF compares decoded source pictures with decoded encoded or remuxed destination pictures, scales the destination look to the reference geometry, and reports mean/minimum scores and scored-frame counts for the selected model (for example `vmaf_4k_v0.6.1`). Pictures are paired in presentation order per track, not by serial or PTS. Report scores retain round-trip double precision; a score alone does not prove that all frames were compared. Inspect the frame count and report status after the job reaches `Done`.

The default VMAF thread count uses all cores. Memory depends on resolution, thread count, simultaneous contexts and queued/unpaired pictures; there is no fixed total-job RAM guarantee. Pass a smaller count as the third constructor argument to reduce extractor memory demand and monitor job telemetry. Two-pass processing such as Degrain requires a rereadable source and is not supported on remux connections.

Write a new filter the same way `Scale` and `Watermark` are written. Do not add public friends so a coordinator can peek.

Destroying a filter graph requests cancellation of its connected endpoints, filters and look decoders before joining workers. Explicit stage stop or failure aborts shared hoppers to wake blocked producers; normal producer EOF still drains queued media and respects the remaining writers.

## Progress

`Transcoder::Progress()` (or `Demuxer::Progress()` in a hand-built tube) returns a shared, read-only clock. Its getters and `Snapshot()` are safe to read from any thread while the job runs; `Snapshot()` captures every value under one lock, so a UI should prefer it to calling getters one by one.

For progress, a positive caller-supplied duration takes precedence. Otherwise, `Plan`
uses the duration reported by the container when it is positive, avoiding a full
packet scan before transcoding. Only when neither is available does it scan the
source. Container durations are estimates and may be inaccurate; they do not
limit how much media is processed. `File::DurationHint()` exposes the available
value without scanning. `File::Duration()` and `File::Duration(observer)` retain
their full-scan behavior, including filling missing stream durations.

A job has up to four independent axes, all measured against the source duration:

| Axis | Getter / `Values` field | Present when | What it measures |
| --- | --- | --- | --- |
| Duration scan | `DurationCalculation()` / `Duration` | Neither a positive supplied duration nor a usable container duration is available | Packet scan that finds the source duration. Empty during CPU-only preparation. |
| Main | `Main()` / `Main`, `MainComplete()` / `MainComplete` | Always | How much of the destination has been written. Exactly 100 once the muxer writes the trailer. |
| Measure | `Measure()` / `Measure`, `MeasureComplete` | A two-pass filter such as `Degrain` is mounted | The first pass over the source. |
| Analytics | `Analytics()` / `Analytics`, `AnalyticsComplete` | An analytics filter such as `VMAF` is mounted | The decoded destination pictures scored so far. |

`Main` is the real transcode percent: measure and analytics never add to it, it never goes backwards, and it is 0 while the duration scan runs. `Measure` and `Analytics` are empty when that phase is not mounted and report their own 0..100 otherwise.

`All()` / `All` is a weighted mix for one-line displays: measure counts 5 %, analytics 10 % and the main job the rest. It stays below 100 until the main job and every mounted phase have finished, so it moves slowly while a first pass runs and can sit near 99 % while analytics drains. Use it, or `Current == Progress::Phase::Complete`, to know when everything has finished, not as the transcode percent. The `std::string` conversion of `Progress` (the CLI status line) uses `All`.

`Current` tells which phase leads: `CalculatingDuration`, `Measure` (first pass running, `Main` still 0), `Processing` and `Complete`.

Edge cases a UI should handle:

- **Main is 100 but analytics is still running.** Analytics decodes and scores the destination on its own path, so it can finish after the muxer. `MainComplete` is true and `Main` is 100 while `AnalyticsComplete` is still false; show the main bar full and the analytics bar still moving.
- **A measure pass is running.** `Main` stays at 0 until the first pass ends; show measure progress separately.
- **The job finished.** When `Status()` is `Done`, `Main` and `All` are both 100, every mounted phase is complete and `Current` is `Complete`. Decide success from `Status()`, not from progress values.

```cpp
if (const auto progress = job.Progress()) {
	const auto values = progress->Snapshot();
	if (values.Current == Progress::Phase::CalculatingDuration)
		ShowScan(values.Duration);                // empty while preparing
	else
		ShowMain(values.Main);                    // real transcode percent
	ShowMeasure(values.Measure, values.MeasureComplete);
	ShowAnalytics(values.Analytics, values.AnalyticsComplete);
	if (values.MainComplete && !values.AnalyticsComplete)
		ShowStatus("Finishing analysis...");
}
```

Custom `EmptyPlan` factories can pass `DurationProgress()` to the observing `Plan` constructor to publish duration scan updates.
That observer also honours `Pause()`, `Resume()` and `Cancel()` during duration calculation,
including cancellation while paused. `File::Duration(observer)` stops scanning when the
callback returns a status other than `Safe::Status::Success`. Completed and interrupted
scans rewind the input reader before returning; interrupted scans are not cached and
can be retried. Observers receive periodic updates even when the source size is unknown.

## Logging

First argument of every `Step` and filter leaf: `StormByte::Safe::Shared<StormByte::Logger::Log>`. Prefer `ThreadedLog` if more than one thread will write.

An empty logger disables logging for manually constructed stages and filters. `Transcoder` requires a nonempty application logger. An ostream used to construct a logger must outlive every stage/job retaining it.

The application logger is scoped at `StormByte/Multimedia/<stage>`. Format is `[%L] %T %c`. Do not put `STMM` or the level name in the payload.

A `Transcoder` job can override `InstallLog` so *its* lines use another path. Tube stages always stay under `StormByte/Multimedia/<stage>`.

Default `Label()` is the producer name. Leaves may still add codec / track in the payload (`Encoder(libx265)`, `Decoder(look t=0)`).

| Level | What Multimedia uses it for |
| --- | --- |
| `LowLevel` | Per-unit wait/wake, DTS, frames. Module Window: 12 lines / 1 s. |
| `Debug` | Binds, reserves, work `n/min/max`. Module Drop: 2/s, burst 4. |
| `Notice` | Created, open, path, eof, closed. Module Drop: 4/s, burst 8. |
| `Info` | `Transcoder` at job close only. |

The application chooses the floor. `LowLevel` is a request for noise and the cost that comes with it. See the [Logger README](https://github.com/StormBytePP/StormByte-Logger) for headers, redaction and the line-lock contract.

## Build options and distribution

Choose bundled or system dependencies and optional features with these CMake options.

| Option | Values | Meaning |
| --- | --- | --- |
| `WITH_FFMPEG` | `BUNDLED` (default) / `SYSTEM` | Use the bundled libav media engine or a compatible system installation. |
| `WITH_VMAF` | `BUNDLED` / `SYSTEM` | Use libvmaf from the same dependency mode as `WITH_FFMPEG`. A differing value is overridden in the CMake cache with a `STATUS` message; mixed bundled/system modes are not used. |
| `WITH_OCR` | `BUNDLED` (default) / `SYSTEM` | Tesseract/Leptonica; Windows forces bundled OCR. |
| `WITH_TESSDATA` | `BUNDLED` (default) / `SYSTEM` | Independent model source: embedded `tessdata_best` selection or installed model files loaded at runtime. |
| `WITH_ZIMG` | `BUNDLED` / `SYSTEM` | zimg dependency used by image processing. |
| `BUILD_SHARED_LIBS` | `ON` (default) / `OFF` | Shared or static Multimedia. StormByte archives follow the same mode. |
| `ENABLE_TEST` | `ON` / `OFF` (default) | Register and build this module's CTest suite. `ENABLE_PGO` turns tests on only in the instrumented tree. |
| `ENABLE_LTO` | `ON` (default) / `OFF` | Link-time optimization for every build type. Combines with `ENABLE_PGO`. |
| `ENABLE_PGO` | `ON` / `OFF` (default) | Train in `pgo-instr`, then compile this directory with the profile. The installed library is the profiled one. |
| `ENABLE_ASAN` | `ON` / `OFF` (default) | Debug ASan/UBSan on supported non-Windows builds; disabled for Release. |
| `WITH_GPL` | `ON` / `OFF` | Allow GPL-licensed components in the bundled media engine, such as x265 and Rubberband. When `OFF`, omit Multimedia's Rubberband and VagueDenoiser sources and installed headers for both bundled and system FFmpeg. It does not control decoding support. |
| `WITH_NONFREE` | `ON` / `OFF` | Allow nonfree components in the bundled media engine, such as FDK-AAC, and dependent Multimedia filters. No current Multimedia filter requires a nonfree component. |

These options change which bundled codec implementations are available; they do not change the license of StormByte-Multimedia. libav is an implementation dependency, not an API you need to call. Review the licenses of the actual dependencies and enabled components when distributing an application.

With `SYSTEM`, the system installation determines codec availability and its associated license terms.

Default Release builds target x86-64-v3 on GCC/Clang x86-64 and AVX2 on MSVC-compatible x86 builds; Apple Silicon targets Apple M1. Explicit user compiler flags remain authoritative. Bundled dependencies also compile specialized SIMD kernels, including AVX512 or optional ARM extensions where supported by the dependency and toolchain, and retain their runtime CPU selectors. Those specialized kernels do not raise the global instruction-set baseline. Bundled FFmpeg x86 builds require a supported NASM assembler rather than silently omitting standalone assembly. System dependencies retain their own build policies.

Codec availability depends on the selected media-engine build and its installed dependencies, not just the registry name. A pinned implementation such as `libx265` must be present in that build. The library's tests exercise media through the StormByte API and do not require separate command-line media tools.

OCR converts bitmap subtitle frames to text when a text subtitle codec is selected. Source language metadata selects the Tesseract model; a missing model is an error, not a translation service. `WITH_TESSDATA` is independent of `WITH_OCR`: either model source can be used with bundled or system Tesseract/Leptonica. Bundled tessdata uses `tessdata_best` LSTM models and embeds only the languages listed in [the tessdata language configuration](thirdparty/tessdata/languages/CMakeLists.txt). Maintain `TESSDATA_LANGUAGES` using base model identifiers only; the generator also includes each available `_vert` variant automatically. The current selection contains 55 base languages and four vertical variants, covering European and major Asian subtitle languages plus Arabic, Hebrew, Turkish and Persian. System tessdata loads model files directly from the directory discovered at configuration time, without embedding them or applying the bundled language list. Set `TESSDATA_DIR`, `TESSDATA_PREFIX` or `Tessdata_ROOT` to select an installation. A partial installation is accepted; requesting an absent language fails at runtime without fallback. Keep the selected directory and required model files available on the machine running the library.

Bitmap subtitle OCR consumes decoded grayscale pixels, not encoded JPEG, PNG or TIFF files, and surrounds the image with white margins for recognition of tightly cropped glyphs. Bundled Tesseract keeps its internal image data as Leptonica `Pix` objects and omits TIFF-backed diagnostic fonts; bundled Leptonica's external image codecs remain disabled.

Typical configure:

```sh
cmake -S . -B build \
  -DWITH_FFMPEG=BUNDLED \
  -DWITH_VMAF=BUNDLED \
  -DWITH_GPL=OFF \
  -DWITH_NONFREE=OFF
```

## Installation

Building from source requires a C++26-capable compiler and standard library, CMake with `CXX_STANDARD 26` support (3.25 or newer), and [StormByte-BuildMaster](https://github.com/StormByte-Suite/StormByte-BuildMaster) 2.1.0 or newer (the `buildmaster` pin). Base, Buffer, Logger and System are not submodules: BuildMaster downloads the release archive for this OS and C++ runtime. A bundled media build still needs the codec submodules (`git clone --recursive`) and, on x86, NASM.

```sh
git clone --recursive https://github.com/StormBytePP/StormByte-Multimedia.git
cd StormByte-Multimedia
cmake -S . -B build
cmake --build build
```

The in-tree CMake target is `StormByte::Multimedia`. The library is `StormByte-Multimedia`; its runtime dependencies must be deployed with a compatible ABI. Include path: the public install prefix, headers as `#include <StormByte/multimedia/file.hxx>`. Run `cmake --install build --prefix <prefix>` after building to install the configured library and headers.

## Tests

The mux-policy batch covers direct FLAC, FLAC in OGA, ALAC in CAF and direct E-AC3 output, plus incompatible FLAC input and WebM attachment rejection. Generated outputs are checked with `File`; no external media tools are invoked.

Additional MP4 cases cover mixed video/audio/timed-text tracks, language and track ordering, `.m4a`/`.m4b` aliases, mixed remux/encode, image cover-art roundtrips and rejection of unsupported font attachments. Cover tests compare payloads and verify that `File::Streams()` and `File::Attachments()` report the expected items. Subtitle cases cover empty tracks and late first cues; container formats may differ in whether an entirely empty track appears in the final file.

Pipeline cases are split into remux, analytics, video, audio, audio-codec, video-codec, watermark, mux-policy, interleave, OCR, negative-input/configuration and decoder-implementation executables under `test/pipeline`. Common helpers are compiled once in a static test support library. Test bodies that call private FFmpeg code share `StormByte-Multimedia-Test`; their executables are lightweight runners. With `BUILD_SHARED_LIBS=ON`, FFmpeg and its codec archives stay inside Multimedia and this single test library, rather than being linked again into every executable. Static Multimedia builds retain their transitive implementation dependencies. Individual CTest names remain `pipeline.test_*`.

Configure with `-DENABLE_TEST=ON`, build, then run CTest from the test registration root:

```sh
cmake -S . -B build-tests -DENABLE_TEST=ON \
	-DWITH_FFMPEG=BUNDLED -DWITH_VMAF=BUNDLED \
	-DWITH_OCR=BUNDLED -DWITH_TESSDATA=BUNDLED
cmake --build build-tests
ctest --test-dir build-tests/test --output-on-failure
```

The suite covers registries, `File`, facade and manual pipelines, remux and transcode workflows, attachments, HDR metadata, OCR, analytics, audio/video codec combinations, filters and invalid input/configuration. Outputs are reopened with `File` to verify stream identity and media properties. Decoder roundtrips create their own inputs and do not depend on another test's output.

Optional cases remain registered across bundled/system and GPL/nonfree configurations. Encoder cases skip when the configured registry has no write support; pinned codec cases also skip unavailable implementations, not encoding failures. This includes FDK-AAC, x264 and x265 without requiring them in system FFmpeg. Bundled builds with `WITH_NONFREE=OFF` skip FDK-AAC encoder and decoder cases before starting a pipeline. The Japanese OCR case runs with both bundled and system OCR; system configurations must provide the Japanese language models. Fixtures are short synthetic media; provenance and font redistribution notices are in [test/files/README.md](test/files/README.md). Passing these cases is not certification of every codec, long-running workload or target platform.

HDR10 metadata cases explicitly select x265, SVT-AV1, libaom, VP9 and the listed HEVC NVENC/QSV/VAAPI implementations. Hardware tests preflight the device and required encoding profile: proven device absence or unsupported backend encoding capability skips, while invalid formats, allocation failures and generic encoding errors remain failures. An independent VAAPI SDR HEVC Main test checks 8-bit encoding even when Main10 is unavailable. Dolby Vision transform cases cover x265, SVT-AV1 and libaom independently, including DOVI-only and DOVI with HDR10. AV1 outputs use regenerated Dolby Vision profile 10. Libaom's HDR10 capability covers color signaling and static metadata preservation through the pipeline and Matroska container; it does not promise static mastering/content-light OBUs in a raw AV1 bitstream.

The watermark case creates `test-output/pipeline/watermark/watermark-logo-path.webm` and `watermark-logo-binary.webm`, using the same logo from a file path and from `#embed` bytes. It prints both paths for manual inspection; the automated checks validate video generation, not the overlay's appearance. Run the `MultimediaPipelineWatermarkTests` executable directly or use `ctest --test-dir build-tests/test -R watermark -V` to see the paths on success. Both outputs use VP9 and do not require GPL/nonfree codecs.

## Contributing

Issues only on this repository. Fork and open a pull request against `master`.

Public API does not grow “because the coordinator needs it”. No new `friend`s. Doxygen on a header is part of the file: update it so it does not lie, do not delete it. Commits are English, one topic, `feat(pipeline): …` / `fix(watermark): …`.

## License

StormByte-Multimedia original source is **dual-licensed**:

1. **GNU Lesser General Public License v3.0 (or later)**  
   Redistribute and/or modify the original source under the LGPL v3 or any later version.  
   Full text: [LICENSE](LICENSE), [COPYING.LGPLv3](COPYING.LGPLv3), <https://www.gnu.org/licenses/lgpl-3.0.html>.

2. **Commercial license**  
   The same original source may be used under a commercial agreement with the copyright holder (David C. Manuelda <StormByte@gmail.com>).  
   That option requires a written agreement. Without it, the LGPL applies.

Both licensing options cover **original StormByte-Multimedia source only**. Third-party components — including FFmpeg, libvmaf and embedded trained data — keep their own licenses and are **not** covered by the commercial grant. See [NOTICE](NOTICE) and `thirdparty/`.

A written StormByte commercial agreement may license the original StormByte-Multimedia code on terms other than the LGPL, including specific use and linking arrangements such as static linking, as stated in that agreement. It does not grant rights to dependencies or waive their license conditions. In particular, enabling `WITH_GPL` or `WITH_NONFREE` can add separate obligations to a binary or other work that uses those components, including for modification, linking (static or dynamic) and redistribution. The packager and final user are responsible for determining and meeting all applicable license and distribution requirements and obtaining any needed patent permissions. A StormByte license does not provide those permissions for GPL or nonfree components and grants no patent rights.

SPDX: `LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial`.

The headers of the public and private trees repeat this grant. When in doubt, those headers and `LICENSE` win over this README.

## Supporting the project

If this saved you from building a separate media pipeline inside your application, a star is the polite nod. A well-aimed issue beats a vague “it broke”. Pull requests that keep the public API focused — `Plan`, `Step`, `Transcoder` and composable filters — are the ones that land.

I wrote this because the alternative was another private transcoder in every product. Maintaining that difference takes evenings.

[Sponsor StormBytePP on GitHub](https://github.com/sponsors/StormBytePP)

Use it. Break it on purpose. Tell me which sentence in this file lied.

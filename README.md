# StormByte

Contributor guides: [Contributing](CONTRIBUTING.md) and [Coding Style](CODING_STYLE.md).

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![C++26](https://img.shields.io/badge/C%2B%2B-26-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.12+-064F8C?logo=cmake&logoColor=white)
![License: LGPL v3 / Commercial](https://img.shields.io/badge/License-LGPL_v3%20%2F%20Commercial-blue.svg)
[![CI](https://github.com/StormBytePP/StormByte-Multimedia/actions/workflows/ci.yml/badge.svg)](https://github.com/StormBytePP/StormByte-Multimedia/actions/workflows/ci.yml)
[![Sponsor](https://img.shields.io/badge/Sponsor-StormBytePP-ea4aaa?logo=githubsponsors)](https://github.com/sponsors/StormBytePP)

**StormByte-Multimedia** is a high-level, extensible C++26 library for building media workflows: inspect files, select streams, remux or transcode them, apply filters, and collect progress and analytics. It uses libav as its media engine, transparently; routine use is expressed entirely through StormByte types such as `File`, `Plan`, `Transcoder`, `Step` and typed filters. You do not need to know libav APIs to build or run a job.

The high-level API does not trade away control. Use `Transcoder` for the common File-to-File workflow, derive from it to add application-specific plans and lifecycle hooks, or wire the same `Step`s by hand when you need a custom graph. Custom filters use StormByte's RAII frame and packet types; only reach for the advanced libav-backed filter surface when those wrappers do not provide an operation your filter needs.

It depends on [StormByte Base](https://github.com/StormBytePP/StormByte), [StormByte Buffer](https://github.com/StormBytePP/StormByte-Buffer) and [StormByte Logger](https://github.com/StormBytePP/StormByte-Logger). Public headers live under `StormByte/multimedia/` and cover the registry, containers, codecs, `File`, and the pipeline (`Plan`, `Step`, `Transcoder`, filters).

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

## Codec compatibility

`Feature::DOVI` indicates that a listed implementation supports Dolby Vision metadata handling for that direction. It is separate from HDR10, HDR10+ and hardware-acceleration support; it does not promise support for every Dolby Vision profile or certify the visual result.

| Direction | Format | Implementations listed with DOVI support |
| --- | --- | --- |
| Decode | HEVC | `hevc` |
| Decode | AV1 | `av1`, `libdav1d` |
| Encode | HEVC | `libx265` (requires a GPL-enabled build and a compatible x265) |
| Encode | AV1 | `libsvtav1`, `libaom-av1` (availability depends on the configured dependencies) |

The registry reflects the capabilities compiled into the selected bundled or system libav build. Decoding remains available independently of GPL encoder components. Encoding requires compatible Dolby Vision input metadata and supported encoder settings; an encoder cannot create a Dolby Vision grade from ordinary HDR pixels alone. Other hardware implementations may decode or encode the picture format without providing the metadata support represented by `Feature::DOVI`.

### Dolby Vision metadata and transforms

Dolby Vision and HDR10 are separate properties: DOVI-only content must not acquire an invented HDR10 fallback. Preserving metadata does not prove that it still describes transformed pixels. Cropping, scaling, overlays, denoising, grading, gamut conversion and tone mapping can make it stale. Validate or regenerate it with an appropriate mastering workflow after such transformations. Neither a successful encode nor metadata preservation proves visual correctness, Dolby conformance or certification.

## Two ways to work

You either let `Transcoder` assemble a job from a fluent map of origin streams, or construct the `Step`s yourself and join them with `operator>>`.

### 1. Transcoder (File → File)

`Transcoder` is the recommended starting point for a File-to-File job. Construct it with source and destination paths, map **output** tracks in mux order, choose remux or encoding, attach filters, and run. The destination container is inferred from the writer path. The stock class is complete for remuxing, transcoding and filtering; inheritance is for adding application-specific behavior, not a prerequisite for ordinary use.

It is also **designed to be inherited**. Override `EmptyPlan()` / `EmptySettled()` for application-specific state, or lifecycle hooks such as `OnProgress`, `OnDone` and `OnError` to connect a UI, scheduler or batch runner. You can extend the job description and orchestration while retaining the same pipeline stages and behavior.

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
	Transcoder job{std::filesystem::path{argv[1]}, std::filesystem::path{argv[2]}, logger};

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
	job.Audio(2).Codec(eac3->get());
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
			std::cout << '\r' << static_cast<std::string>(*progress) << std::flush;
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
| `Audio(2).Codec(eac3->get())` | Recode that origin audio. |
| `.Implementation(ImplementationSide::Decoder, String{"libdav1d"})` | Pin the source decoder on a recode track; unavailable or codec-mismatched names fail without fallback. |
| `Ignore(n)` | Drop origin stream `n`. |
| `Attachments()` / `Attachments("image/png")` | Keep all attachments, or only a MIME. Default without a call is drop. |
| `Transcoder(source, destination, logger)` | Supplies locations before mapping; the writer path determines the container. |
| `Filter<Analytics>(…)` on the **job** | Global analytics on matching encode or remux lanes; `Track::Filter` also accepts track-scoped analytics. |

`Run()` is asynchronous. `Pause()` / `Resume()` / `Cancel()` talk to the coordinator. After `Done`, `Reports()` holds analytics snapshots (VMAF mean/min and anything else you attached). Mux close is not analytics EOF: `Transcoder` waits for the route to go idle before `OnDone` / `Reports`.

When source duration is not supplied, `Progress` first displays `Calculating duration` with an activity indicator. Duration analysis happens before processing begins; after it completes, progress reports the scan and processing phases separately. `Progress::Snapshot()` gives applications a consistent phase, duration/analytics percentages, processing score and completion flags, so a UI does not need to parse the status string. Getters and snapshots are safe to read concurrently. Custom `EmptyPlan` factories can pass `DurationProgress()` to the observing `Plan` constructor to publish scan updates.

Capture `job.Telemetry()` before `Run()` if the final snapshot must outlive the job. It retains each stage's origin, lifecycle, frame/packet input and output counts, setup and elapsed time, Process-call total/mean/min/max, and blocked-wait total/count/max. The coordinator also logs the final multi-line report at `Info`; converting the retained handle to `std::string` produces the same report. A hand-built `Step` or filter exposes its own retained `Telemetry()` handle.

`JobTelemetry::PeakMemory()` (also `MemoryMaximum()`) is the highest sampled process resident set size in bytes; `MemoryMinimum()` and `MemoryCurrent()` report the lowest sample and last sample. The coordinator samples nominally every 20 ms and at job start/end. These values are process-wide RSS, not memory attributed to an individual stage; OS sampling may miss short-lived peaks and can be unavailable on unsupported platforms.

Quality knobs on a recode track are the obvious ones: `CRF`, `BitRate`, `MaxBitRate`, `Preset`, `Tune`, `FineTune`, plus `Language` / `Title` overrides.

`Implementation(ImplementationSide::Encoder, String{"libsvtav1"})` selects the encoder; `Implementation(ImplementationSide::Decoder, String{"libdav1d"})` independently selects the decoder. The one-argument `Implementation(name)` remains shorthand for the encoder. An empty name restores default selection for that side. Pins must match the destination and source codecs respectively. A manual pipeline can set `Config::Implementation::Decoder` on the Plan track or call `Decoder::Implementation` before wiring; an explicit stage pin takes precedence over the Plan pin. Remux does not open an origin decoder and ignores these pins.

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

#include <chrono>
#include <filesystem>
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

	Plan plan{std::filesystem::path{"in.mkv"}, std::filesystem::path{"out.mkv"}};
	Config::Video video;
	video.Codec(hevc->get());
	plan.add(Track{0, std::move(video)});
	if (auto check = plan.Check(); !check) {
		std::cerr << check.error()->what() << '\n';
		return 1;
	}

	Demuxer demux(logger);
	Decoder decode(logger, 0);
	Encoder encode(logger, 0, hevc->get());
	Muxer mux(logger);
	std::move(plan) >> demux;
	demux >> mux;
	demux >> decode;
	decode >> encode;
	encode >> mux;

	while (mux.Status() != State::Stopped && !mux.Failed()) {
		if (demux.Failed() || decode.Failed() || encode.Failed()) {
			mux.Stop();
			return 1;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return demux.Failed() || decode.Failed() || encode.Failed() || mux.Failed() ? 1 : 0;
}
```

`operator>>` shares the `Plan` and binds hoppers. `Demuxer` produces `Packet`s and receives nothing. `Decoder` turns those into `Frame`s. `Encoder` produces `Packet`s again. `Muxer` reserves the output slot — remux does not. Fan-out from one demuxer to several decoders / remuxers is the same operator.

`Filters` sits between two `StormByte::Safe::Shared<Step>` ends when you need a filter chain or analytics. `Transcoder` builds that graph for you. By hand:

```cpp
#include <StormByte/multimedia/pipeline/filters.hxx>
#include <StormByte/multimedia/pipeline/filters/analytics/vmaf.hxx>
#include <StormByte/multimedia/pipeline/filters/video/scale.hxx>

auto decode = StormByte::Safe::MakeShared<Decoder>(logger, 0);
auto encode = StormByte::Safe::MakeShared<Encoder>(logger, 0, hevc->get());
Filters graph;
graph.Between(decode, encode).Add<Filter::Video::Scale>(logger, 1920u, 1080u);
graph.Add<Filter::Video::VMAF>(logger, StormByte::Safe::String{"vmaf_4k_v0.6.1"});	// one node, every matching stretch
graph.Close();
```

## Plan, items and the tube contract

- **`Plan`** describes the job: input/output locations, a source-file snapshot and the **output** tracks. The destination container is inferred from the writer path. `Check()` validates the plan's structure; codec and format availability are confirmed when the job is configured.
- **`Packet`** is a compressed access unit. **`Frame`** is a decoded one. No public timing setters. Mutate pixels through `Decoder` / `Encoder` / a filter `Replace`, not a setter on `Frame`.
- **`Serial`** is a monotone id assigned by the tube. Public getter, no setter. It is not a frame count.
- **`Remuxer`** forwards compressed packets and adapts them to the destination. “Copy” as a stage does not exist.
- **Caps** (hopper capacity and stage ceilings) limit individual queues, not total job memory. Do not treat EOF as Fail. Recoverable logo failures can become Watermark passthrough; invalid plans, unsupported encoders and fatal processing errors remain failures.
- **Frames and metadata** — filters can replace decoded content while the pipeline preserves the timing and properties it can safely retain. HDR and Dolby Vision metadata is not generally recalibrated to describe changed pixels; see [Dolby Vision metadata and transforms](#dolby-vision-metadata-and-transforms).

## DLL boundaries

The public API uses `StormByte::Safe` values (`String`, `Optional`), collections (`Vector`, `Map`, `Pair`) and owners (`Shared`, `Unique`) for owning data that crosses module boundaries. Owners retain provider-local release operations so the exact payload is destroyed by its creating provider. Construct shared owners with `Safe::MakeShared<T>(…)`; parameters typed as `Safe::String` require explicit `Safe::String{"text"}` construction. Fields holding references to external registry entries are borrowed, not owned: those registries must remain valid for the lifetime of the references.

`STORMBYTE_DECLARE_MAYBE_SAFE` is a provider's responsibility and promise, not an automatic audit: a declared type must keep its members, special members and heap-affecting operations boundary-safe. Consumers and providers still need a compatible C++/STL ABI. All providers supplying live values, owners or callbacks must remain loaded until those objects are released; this is neither arbitrary-ABI compatibility nor a safe-unload guarantee.

For custom payloads, derived `Transcoder` providers must override `EmptyPlan()` / `EmptySettled()` in their own module and create the exact derived payload with `Safe::Shared<Plan>::MakePointer<DerivedPlan>(…)` / `Safe::Unique<TrackSettled>::MakePointer<DerivedSettled>(…)`. Derived `Plan` types must override `Move()` there; derived `TrackSettled` types must override both `Clone()` and `Move()` there. Preserve the dynamic type and its provider-local release operations rather than slicing to the base. `Plan::Clone()` is not supported.

## Filters and analytics

Container output is selected from the destination path and follows the format's supported stream and attachment rules. Matroska, WebM and MP4 support the common media workflows; image attachments can be written as cover art in MP4, while unsupported attachment types are reported rather than silently discarded. The library writes its application identity into supported container metadata. Formats that require multiple output files are outside the single-writer workflow.

Filters are leaves, not a second pipeline language. `Scale` is resize (that is the name). `Watermark` is a still image on decoded video, with Hold so a black slate at the start does not pin the letterbox probe too early.

Filters retain reusable scaling graphs and workers in their own provider-owned cache on every platform. For repeated direct frame scaling outside a filter, retain an `FFmpeg::AVFrame::ScaleContext` and call `source.ScaleTo(context, destination, width, height, filter, scaler)`. Use a context from only one thread at a time and destroy it during ordinary application teardown, not from a thread-local destructor or DLL detach callback. The original `ScaleTo` overload remains available for one-off calls but does not retain a cache across calls.

`Degrain` is an experimental two-pass regional film-grain reducer. Attach it with `.Filter<Degrain>(logger)` or `.Filter<Degrain>(logger, sigmaCap)` before sharpening or scaling. Measurement uses a 12 x 8 grid and up to five neighbouring pictures on each side, compensates local brightness drift, and conservatively suppresses filtering for motion, texture and skin-like colours. Darkness alone never forces filtering. Clean or inconclusive frames, missing/duplicate measurement timestamps, and unsupported pixel layouts pass through unchanged. The default sigma ceiling is 4.0 in 8-bit-equivalent units; zero disables filtering, finite ceilings are clamped to [0, 100], and non-finite ceilings fail the filter.

Degrain blends the original and up to three bracketing filtered strengths using spatially smooth regional targets. It supports software planar integer YUV/gray at 8/10/12/16 bits in either byte order, preserving geometry, format, original PTS, metadata and alpha. Denoising uses the previous real input and current input, with current duplicated in place of a future picture; detected discontinuities use current alone. This asymmetric temporal baseline produces one output per input without delay, but is not motion compensation. Brightness cuts/flashes isolate temporal smoothing; equal-brightness cuts, fine texture and correlated/compressed grain remain heuristic limitations. Measurement holds at most eleven pictures plus compact per-frame target maps; repeated filtering is expensive. Do not stack it with another grain-denoising filter.

Analytics do not change the media output. VMAF compares decoded source pictures with decoded encoded or remuxed destination pictures, scales the destination look to the reference geometry, and reports mean/minimum scores and scored-frame counts for the selected model (for example `vmaf_4k_v0.6.1`). Pictures are paired in presentation order per track, not by serial or PTS. Report scores retain round-trip double precision; a score alone does not prove that all frames were compared. Inspect the frame count and report status after the job reaches `Done`.

The default VMAF thread count uses all cores. Memory depends on resolution, thread count, simultaneous contexts and queued/unpaired pictures; there is no fixed total-job RAM guarantee. Pass a smaller count as the third constructor argument to reduce extractor memory demand and monitor job telemetry. Two-pass processing such as Degrain requires a rereadable source and is not supported on remux connections.

Write a new filter the same way `Scale` and `Watermark` are written. Do not add public friends so a coordinator can peek.

Destroying a filter graph requests cancellation of its connected endpoints, filters and look decoders before joining workers. Explicit stage stop or failure aborts shared hoppers to wake blocked producers; normal producer EOF still drains queued media and respects the remaining writers.

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
| `WITH_VMAF` | `BUNDLED` (default) / `SYSTEM` | Use bundled or system libvmaf for VMAF analytics. |
| `WITH_OCR` | `BUNDLED` (default) / `SYSTEM` | Tesseract/Leptonica; Windows forces bundled OCR. |
| `WITH_TESSDATA` | `BUNDLED` / `SYSTEM` | OCR language models; selected languages must be installed and discoverable. |
| `WITH_ZIMG` | `BUNDLED` / `SYSTEM` | zimg dependency used by image processing. |
| `BUILD_SHARED_LIBS` | `ON` (default) / `OFF` | Shared or static Multimedia and StormByte libraries. |
| `ENABLE_TEST` | `ON` / `OFF` (default) | Register and build this module's CTest suite. |
| `ENABLE_ASAN` | `ON` / `OFF` (default) | Debug ASan/UBSan on supported non-Windows builds; disabled for Release. |
| `WITH_GPL` | `ON` / `OFF` | Allow GPL-licensed components in the bundled media engine; this enables codecs such as x265 when available. It does not control decoding support. |
| `WITH_NONFREE` | `ON` / `OFF` | Allow nonfree components in the bundled media engine, such as FDK-AAC. |

These options change which bundled codec implementations are available; they do not change the license of StormByte-Multimedia. libav is an implementation dependency, not an API you need to call. Review the licenses of the actual dependencies and enabled components when distributing an application.

With `SYSTEM`, the system installation determines codec availability and its associated license terms.

Codec availability depends on the selected media-engine build and its installed dependencies, not just the registry name. A pinned implementation such as `libx265` must be present in that build. The library's tests exercise media through the StormByte API and do not require separate command-line media tools.

OCR converts bitmap subtitle frames to text when a text subtitle codec is selected. Source language metadata selects the Tesseract model; a missing model is an error, not a translation service. Bundled tessdata packages language models separately from the OCR engine. System tessdata requires an installation discoverable by the configured paths or Tesseract environment.

Typical configure:

```sh
cmake -S . -B build \
  -DWITH_FFMPEG=BUNDLED \
  -DWITH_VMAF=BUNDLED \
  -DWITH_GPL=OFF \
  -DWITH_NONFREE=OFF
```

## Installation

Building from source requires a C++26-capable compiler and standard library, CMake with `CXX_STANDARD 26` support (3.25 or newer), and the StormByte modules listed above. The root currently declares an older CMake minimum; that declaration does not remove the newer language-standard requirement. A bundled build also builds its media dependencies, so the required platform toolchain may include additional tools.

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

Pipeline cases are split into remux, analytics, video, audio, audio-codec, video-codec, watermark, mux-policy, OCR, negative-input/configuration and decoder-implementation executables under `test/pipeline`. Common helpers are compiled once in a static test support library; category-local edits rebuild only the affected executable. Individual CTest names remain `pipeline.test_*`.

Configure with `-DENABLE_TEST=ON`, build, then run CTest from the test registration root:

```sh
cmake -S . -B build-tests -DENABLE_TEST=ON \
	-DWITH_FFMPEG=BUNDLED -DWITH_VMAF=BUNDLED \
	-DWITH_OCR=BUNDLED -DWITH_TESSDATA=BUNDLED
cmake --build build-tests
ctest --test-dir build-tests/test --output-on-failure
```

The suite covers registries, `File`, facade and manual pipelines, remux and transcode workflows, attachments, HDR metadata, OCR, analytics, audio/video codec combinations, filters and invalid input/configuration. Outputs are reopened with `File` to verify stream identity and media properties. Decoder roundtrips create their own inputs and do not depend on another test's output.

Encoder cases can skip when the configured registry has no write support; pinned codec cases skip only unavailable implementations, not encoding failures. FDK-AAC is required when the bundled build enables nonfree components. Fixtures are short synthetic media; provenance and font redistribution notices are in [test/files/README.md](test/files/README.md). Passing these cases is not certification of every codec, long-running workload or target platform.

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

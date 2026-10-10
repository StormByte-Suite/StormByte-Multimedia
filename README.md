# StormByte

Contributor guides: [Contributing](CONTRIBUTING.md) and [Coding Style](CODING_STYLE.md).

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![C++26](https://img.shields.io/badge/C%2B%2B-26-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.12+-064F8C?logo=cmake&logoColor=white)
![License: LGPL v3 / Commercial](https://img.shields.io/badge/License-LGPL_v3%20%2F%20Commercial-blue.svg)
[![CI](https://github.com/StormBytePP/StormByte-Multimedia/actions/workflows/ci.yml/badge.svg)](https://github.com/StormBytePP/StormByte-Multimedia/actions/workflows/ci.yml)
[![Sponsor](https://img.shields.io/badge/Sponsor-StormBytePP-ea4aaa?logo=githubsponsors)](https://github.com/sponsors/StormBytePP)

This repository is **StormByte Multimedia**: a C++26 pipeline for decoding, filtering, encoding and muxing media on top of FFmpeg (`libav`). It is **not** a thin wrapper around raw FFmpeg contexts. Codec and format backends remain private; filter-facing frame, packet and graph adapters are part of the public surface where needed.

It depends on [StormByte Base](https://github.com/StormBytePP/StormByte), [StormByte Buffer](https://github.com/StormBytePP/StormByte-Buffer) and [StormByte Logger](https://github.com/StormBytePP/StormByte-Logger). Public headers live under `StormByte/multimedia/` and cover the registry, containers, codecs, `File`, and the pipeline (`Plan`, `Step`, `Transcoder`, filters).

The suite is split on purpose. Base, Buffer, Config, Crypto, Database, Logger, Network and System are **other repositories**. This one does not implement them.

## What this module does

- **A closed job intention** — `Plan` owns the reader and writer through `Safe::Unique`, a consultation `File` snapshot and the **output** track list. The destination container is resolved from the writer path extension. `add` order is mux order. Omit a stream and it is dropped. `Check()` asks whether the intention is well formed, not whether FFmpeg will succeed.
- **A tube of workers** — `Plan >> Demuxer >> (Decoder | Remuxer) [>> Filters] >> Encoder? >> Muxer`. Each `Step` is a worker with hoppers. Items are `Packet` (compressed AU) or `Frame` (decoded AU). Timing has no public setters. `Serial` is a monotone tube id, not `nb_frames`.
- **Two ways in** — `Transcoder` is the File→File facade (inheritable, hookable, zero hacks). The same tube can be wired by hand with `operator>>`. Anything `Transcoder` can do, a hand-built tube can do. If a user-built tube fails, `Transcoder` fails the same way.
- **Registry** — codec/container identities and operations available in this build. Look up `"H.265"` / `"hevc"` or `"Matroska"` / `"matroska"` and check access before selecting an encoder. Output uses the actual writer filename to select the FFmpeg muxer. Registered single-file destinations use generic libavformat writing by default, including FLAC, CAF and E-AC3; codec and stream-count restrictions come from FFmpeg, not a Multimedia whitelist. Registry presence alone does not guarantee that a destination accepts every codec or track combination.
- **Filters** — typed leaves on decoded frames or compressed packets (`Scale`, `Watermark`, analytics / VMAF, …). Recoverable conditions follow each filter's contract: a missing Watermark logo can become passthrough, while invalid configuration or processing failures can fail a stage. Check job status and analytics reports rather than assuming every warning or filter failure is harmless.
- **Logging** — every `Step` takes a `StormByte::Safe::Shared<StormByte::Logger::Log>` (prefer `ThreadedLog`). Lines use component `StormByte/Multimedia/<stage>` (`Demuxer`, `Transcoder`, `Watermark`, …) and format `[%L] %T %c`. The print floor belongs to the **application**. Module throttle: Window on LowLevel, Drop on Debug and Notice. Warning / Error / Fatal are not throttled.

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

- This README: how to build, the two entry points, the tube contract, distribution flags.
- Doxygen class reference (headers under `StormByte/multimedia/`): [https://suite.stormbyte.org/StormByte-Multimedia/](https://suite.stormbyte.org/StormByte-Multimedia/).
- Logger contract used by every `Step`: [https://suite.stormbyte.org/StormByte-Logger/](https://suite.stormbyte.org/StormByte-Logger/).

## Two ways to work

You either let `Transcoder` assemble a job from a fluent map of origin streams, or construct the `Step`s yourself and join them with `operator>>`.

### 1. Transcoder (File → File)

`Transcoder` is the facade most applications want. Construct it with source and destination paths (or owned reader/writer locations), then name **output** tracks in mux order, attach filters and run the coordinator. The destination container is inferred from the writer path extension. The stock class is complete: you do not have to derive anything to remux, recode or filter.

It is also **designed to be inherited**. Override `EmptyPlan()` / `EmptySettled()` to carry your own fields, or the hooks (`OnConfigure`, `OnStart`, `OnPlan`, `OnSettled`, `OnProgress`, `OnDone`, `OnError`, `OnAborted`) to drive a UI or a batch runner. Override `InstallLog()` so this job’s own lines use another component path; tube stages stay under `StormByte/Multimedia/<stage>`. Hooks are not an escape hatch around the tube. If a hand-wired tube cannot do it, `Transcoder` will not sneak it in.

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

When source duration is not supplied, `Progress` first displays only `Calculating duration` with an animated activity indicator. During source preparation and FFmpeg stream analysis there is no percentage: this work can consume CPU without advancing through the file. Once packet scanning starts, the activity indicator freezes and the line adds a monotone estimate based on processed packet positions relative to source size, not on AVIO seeks or read-ahead; formats without packet positions use accumulated packet payload bytes as an approximation. The 100 percent value is reserved for EOF followed by a successful rewind. Processing and analytics remain inactive until the scan and rewind finish; this preliminary percentage is separate from `All()`. The reader's cache and read-ahead settings are unchanged. `Progress::Snapshot()` captures the exclusive phase, optional duration/measure/analytics percentages, combined processing score, and completion flags under one lock, so applications can render their own UI without parsing the status string. During preparation its phase is `CalculatingDuration` and its `Duration` value is empty. Getters and snapshots are safe to read concurrently. Custom `EmptyPlan` factories can pass `DurationProgress()` to the observing `Plan` constructor to publish scan updates.

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

- **`Plan`** is the whole job. Owned reader/writer locations, a consultation `File` snapshot, and a destination container resolved from the writer path. `Tracks` is the list of **outputs**. `Check()` is shape, not a rehearsal of FFmpeg.
- **`Packet`** is a compressed access unit. **`Frame`** is a decoded one. No public timing setters. Mutate pixels through `Decoder` / `Encoder` / a filter `Replace`, not a setter on `Frame`.
- **`Serial`** is a monotone id assigned by the tube. Public getter, no setter. It is not a frame count.
- **`Remuxer`** forwards compressed packets and adapts them to the destination. “Copy” as a stage does not exist.
- **Caps** (hopper capacity and stage ceilings) limit individual queues, not total job memory. Do not treat EOF as Fail. Recoverable logo failures can become Watermark passthrough; invalid plans, unsupported encoders and fatal processing errors remain failures.
- **Content** behind `Frame` is virtual (passthrough / video / audio). After `Scale`, HDR10+ and friends are recalculated on `Replace`. Metadata is not dropped by `memcmp`.

## DLL boundaries

The public API uses `StormByte::Safe` values (`String`, `Optional`), collections (`Vector`, `Map`, `Pair`) and owners (`Shared`, `Unique`) for owning data that crosses module boundaries. Owners retain provider-local release operations so the exact payload is destroyed by its creating provider. Construct shared owners with `Safe::MakeShared<T>(…)`; parameters typed as `Safe::String` require explicit `Safe::String{"text"}` construction. Fields holding references to external registry entries are borrowed, not owned: those registries must remain valid for the lifetime of the references.

`STORMBYTE_DECLARE_MAYBE_SAFE` is a provider's responsibility and promise, not an automatic audit: a declared type must keep its members, special members and heap-affecting operations boundary-safe. Consumers and providers still need a compatible C++/STL ABI. All providers supplying live values, owners or callbacks must remain loaded until those objects are released; this is neither arbitrary-ABI compatibility nor a safe-unload guarantee.

For custom payloads, derived `Transcoder` providers must override `EmptyPlan()` / `EmptySettled()` in their own module and create the exact derived payload with `Safe::Shared<Plan>::MakePointer<DerivedPlan>(…)` / `Safe::Unique<TrackSettled>::MakePointer<DerivedSettled>(…)`. Derived `Plan` types must override `Move()` there; derived `TrackSettled` types must override both `Clone()` and `Move()` there. Preserve the dynamic type and its provider-local release operations rather than slicing to the base. `Plan::Clone()` is not supported.

## Filters and analytics

Muxing shares one FFmpeg backend with private, separately maintained policies for Matroska, WebM and MOV/MP4. Matroska retains attachment streams, header metadata, default dispositions and interleaving adaptations; WebM shares its container machinery but rejects file attachments. MOV/MP4 retains default stream dispositions. Other muxers keep FFmpeg defaults. The `StormByte-Multimedia` writing-app tag is retained. Formats that manage their own files or require multiple outputs are not supported by the single buffered-writer contract.

Filters are leaves, not a second pipeline language. `Scale` is resize (that is the name). `Watermark` is a still image on decoded video, with Hold so a black slate at the start does not pin the letterbox probe too early.

`Degrain` is an experimental two-pass regional film-grain reducer. Attach it with `.Filter<Degrain>(logger)` or `.Filter<Degrain>(logger, sigmaCap)` before sharpening or scaling. Measurement uses a 12 x 8 grid and up to five neighbouring pictures on each side, compensates local brightness drift, and conservatively suppresses filtering for motion, texture and skin-like colours. Darkness alone never forces filtering. Clean or inconclusive frames, missing/duplicate measurement timestamps, and unsupported pixel layouts pass through unchanged. The default sigma ceiling is 4.0 in 8-bit-equivalent units; zero disables filtering, finite ceilings are clamped to [0, 100], and non-finite ceilings fail the filter.

Degrain blends the original and up to three bracketing filtered strengths using spatially smooth regional targets. It supports software planar integer YUV/gray at 8/10/12/16 bits in either byte order, preserving geometry, format, original PTS, metadata and alpha. Denoising uses the previous real input and current input, with current duplicated in place of a future picture; detected discontinuities use current alone. This asymmetric temporal baseline produces one output per input without delay, but is not motion compensation. Brightness cuts/flashes isolate temporal smoothing; equal-brightness cuts, fine texture and correlated/compressed grain remain heuristic limitations. Measurement holds at most eleven pictures plus compact per-frame target maps; repeated filtering is expensive. Do not stack it with another grain-denoising filter.

Analytics do not change the media output. VMAF compares decoded source pictures with decoded encoded or remuxed destination pictures, scales the destination look to the reference geometry, and reports mean/minimum scores and scored-frame counts for the selected model (for example `vmaf_4k_v0.6.1`). Pictures are paired in presentation order per track, not by serial or PTS. Report scores retain round-trip double precision; a score alone does not prove that all frames were compared. Inspect the frame count and report status after the job reaches `Done`.

The default VMAF thread count uses all cores. Memory depends on resolution, thread count, simultaneous contexts and queued/unpaired pictures; there is no fixed total-job RAM guarantee. Pass a smaller count as the third constructor argument to reduce extractor memory demand and monitor job telemetry. Two-pass processing such as Degrain requires a rereadable source and is not supported on remux connections.

Write a new filter the same way `Scale` and `Watermark` are written. Do not add public friends so a coordinator can peek.

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

Third-party trees live under `thirdparty/` and are wired through [StormByte BuildMaster](https://github.com/StormBytePP/StormByte-BuildMaster).

| Option | Values | Meaning |
| --- | --- | --- |
| `WITH_FFMPEG` | `BUNDLED` (default) / `SYSTEM` | Nested Meson FFmpeg, or `FindFFmpeg` against the host. |
| `WITH_VMAF` | `BUNDLED` (default) / `SYSTEM` | Nested libvmaf, or `FindVmaf` (`libvmaf-dev` on Debian; Ubuntu archives do not ship it). |
| `WITH_OCR` | `BUNDLED` (default) / `SYSTEM` | Tesseract/Leptonica; Windows forces bundled OCR. |
| `WITH_TESSDATA` | `BUNDLED` / `SYSTEM` | OCR language models; selected languages must be installed and discoverable. |
| `WITH_ZIMG` | `BUNDLED` / `SYSTEM` | zimg dependency used by image processing. |
| `BUILD_SHARED_LIBS` | `ON` (default) / `OFF` | Shared or static Multimedia and StormByte libraries. |
| `ENABLE_TEST` | `ON` / `OFF` (default) | Register and build this module's CTest suite. |
| `ENABLE_ASAN` | `ON` / `OFF` (default) | Debug ASan/UBSan on supported non-Windows builds; disabled for Release. |
| `WITH_GPL` | `ON` / `OFF` | GPL components inside bundled FFmpeg (`gpl=enabled`, `version3=enabled`). |
| `WITH_NONFREE` | `ON` / `OFF` | Nonfree components inside bundled FFmpeg. |

`WITH_GPL` and `WITH_NONFREE` change **what the bundled FFmpeg is allowed to compile**. They do not relicense StormByte-Multimedia. If you ship a binary linked against a GPL or nonfree FFmpeg, **that binary** follows FFmpeg’s license combination. Leave both `OFF` when you need a redistributable build that stays on the LGPL side of FFmpeg.

`SYSTEM` FFmpeg is whatever the host already linked; you inherit that host’s license surface.

Codec availability depends on the selected FFmpeg build and its external libraries, not just the registry name. A pinned implementation such as `libx265` must exist in that build. Keep system FFmpeg and system libvmaf ABI-compatible, including any FFmpeg codec dependencies that themselves link libvmaf. Bundled FFmpeg builds libraries with its programs and tests disabled; Multimedia tests do not require the `ffmpeg` or `ffprobe` executables.

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

Needs a C++26-capable compiler and standard library, CMake with `CXX_STANDARD 26` support (3.25 or newer), and the StormByte modules listed above. The root currently declares an older CMake minimum; that declaration does not remove the newer language-standard requirement. Bundled FFmpeg also needs its platform build tools, including an assembler on relevant x86 builds, and Meson/Ninja through BuildMaster.

```sh
git clone --recursive https://github.com/StormBytePP/StormByte-Multimedia.git
cd StormByte-Multimedia
cmake -S . -B build
cmake --build build
```

The in-tree CMake target is `StormByte::Multimedia`. The library is `StormByte-Multimedia`; its runtime dependencies must be deployed with a compatible ABI. Include path: the public install prefix, headers as `#include <StormByte/multimedia/file.hxx>`. Run `cmake --install build --prefix <prefix>` after building to install the configured library and headers.

## Tests

The mux-policy batch covers direct FLAC, FLAC in OGA, ALAC in CAF and direct E-AC3 output, plus incompatible FLAC input and WebM attachment rejection. Generated outputs are checked with `File`; no external media tools are invoked.

Pipeline cases are split into remux, analytics, video, audio, OCR, negative-input/configuration and decoder-implementation executables under `test/pipeline`. Common helpers are compiled once in a static test support library; category-local edits rebuild only the affected executable. Individual CTest names remain `pipeline.test_*`.

Configure with `-DENABLE_TEST=ON`, build, then run CTest from the test registration root:

```sh
cmake -S . -B build-tests -DENABLE_TEST=ON \
	-DWITH_FFMPEG=BUNDLED -DWITH_VMAF=BUNDLED \
	-DWITH_OCR=BUNDLED -DWITH_TESSDATA=BUNDLED
cmake --build build-tests
ctest --test-dir build-tests/test --output-on-failure
```

The initial suite has 49 cases covering registries, fixed `File` properties, facade/manual remux, attachment inclusion/omission, HDR encoding, Japanese PGS OCR, exact VMAF remux reports, audio conversions and invalid input/configuration. Encoder cases can skip when the configured registry has no write support; other missing prerequisites must not be treated as success. Each case has a 30-second timeout. Fixtures are short synthetic media; provenance and font redistribution notices are in [test/files/README.md](test/files/README.md). Passing these cases is not certification of every codec, long-running workload or target platform.

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

Both licenses cover **original StormByte-Multimedia source only**. Third-party components — including FFmpeg, libvmaf and embedded trained data — keep their own licenses and are **not** covered by the commercial grant. See [NOTICE](NOTICE) and `thirdparty/`.

Neither license grants patent rights. SPDX: `LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial`.

The headers of the public and private trees repeat this grant. When in doubt, those headers and `LICENSE` win over this README.

## Supporting the project

If this saved you from another pile of raw `AVCodecContext` and a private graph of `av_read_frame` loops, a star is the polite nod. A well-aimed issue beats a vague “it broke”. Pull requests that keep the public tube small — `Plan`, `Step`, `Transcoder`, filters as leaves — are the ones that land.

I wrote this because the alternative was another private transcoder in every product. Maintaining that difference takes evenings.

[Sponsor StormBytePP on GitHub](https://github.com/sponsors/StormBytePP)

Use it. Break it on purpose. Tell me which sentence in this file lied.

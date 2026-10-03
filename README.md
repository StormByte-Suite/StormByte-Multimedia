# StormByte

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![C++26](https://img.shields.io/badge/C%2B%2B-26-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.12+-064F8C?logo=cmake&logoColor=white)
![License: LGPL v3 / Commercial](https://img.shields.io/badge/License-LGPL_v3%20%2F%20Commercial-blue.svg)
[![CI](https://github.com/StormBytePP/StormByte-Multimedia/actions/workflows/ci.yml/badge.svg)](https://github.com/StormBytePP/StormByte-Multimedia/actions/workflows/ci.yml)
[![Sponsor](https://img.shields.io/badge/Sponsor-StormBytePP-ea4aaa?logo=githubsponsors)](https://github.com/sponsors/StormBytePP)

This repository is **StormByte Multimedia**: a C++26 pipeline for decoding, filtering, encoding and muxing media on top of FFmpeg (`libav`). It is **not** a thin wrapper around `AVFrame` / `AVPacket`. Those types never leave the private tree.

It depends on [StormByte Base](https://github.com/StormBytePP/StormByte), [StormByte Buffer](https://github.com/StormBytePP/StormByte-Buffer) and [StormByte Logger](https://github.com/StormBytePP/StormByte-Logger). Public headers live under `StormByte/multimedia/` and cover the registry, containers, codecs, `File`, and the pipeline (`Plan`, `Step`, `Transcoder`, filters).

The suite is split on purpose. Base, Buffer, Config, Crypto, Database, Logger, Network and System are **other repositories**. This one does not implement them.

## What this module does

- **A closed job intention** — `Plan` owns the origin `File` (move-only), the destination `Container`, the output path and the **output** track list. `add` order is mux order. Omit a stream and it is dropped. `Check()` asks whether the intention is well formed, not whether FFmpeg will succeed.
- **A tube of workers** — `Plan >> Demuxer >> (Decoder | Remuxer) [>> Filters] >> Encoder? >> Muxer`. Each `Step` is a worker with hoppers. Items are `Packet` (compressed AU) or `Frame` (decoded AU). Timing has no public setters. `Serial` is a monotone tube id, not `nb_frames`.
- **Two ways in** — `Transcoder` is the File→File facade (inheritable, hookable, zero hacks). The same tube can be wired by hand with `operator>>`. Anything `Transcoder` can do, a hand-built tube can do. If a user-built tube fails, `Transcoder` fails the same way.
- **Registry** — codecs and containers that actually exist in this build. Look up `"H.265"` / `"hevc"` or `"Matroska"` / `"matroska"`. Missing name is an error, not a silent fallback.
- **Filters** — typed leaves on decoded frames or compressed packets (`Scale`, `Watermark`, analytics / VMAF, …). A bad filter is a Warning and the job continues. A broken tube frame is a Fail.
- **Logging** — every `Step` takes a `std::shared_ptr<StormByte::Logger::Log>` (prefer `ThreadedLog`). Lines use component `StormByte/Multimedia/<stage>` (`Demuxer`, `Transcoder`, `Watermark`, …) and format `[%L] %T %c`. The print floor belongs to the **application**. Module throttle: Window on LowLevel, Drop on Debug and Notice. Warning / Error / Fatal are not throttled.

## The rest of the suite

| Module | Role | API |
| --- | --- | --- |
| [Base](https://github.com/StormBytePP/StormByte) | Exceptions, Expected, serialization, strings, UUID, concepts | [/StormByte](https://dev.stormbyte.org/StormByte) |
| [Buffer](https://github.com/StormBytePP/StormByte-Buffer) | FIFO, SharedFIFO, Ring, Producer/Consumer and multi-stage pipelines | [/StormByte-Buffer](https://dev.stormbyte.org/StormByte-Buffer) |
| [Config](https://github.com/StormBytePP/StormByte-Config) | Human-readable text and versioned binary documents (groups, lists, raw bytes) | [/StormByte-Config](https://dev.stormbyte.org/StormByte-Config) |
| [Crypto](https://github.com/StormBytePP/StormByte-Crypto) | Hash, compress, encrypt, sign and key agreement — Crypto++ never leaves the private tree | [/StormByte-Crypto](https://dev.stormbyte.org/StormByte-Crypto) |
| [Database](https://github.com/StormBytePP/StormByte-Database) | One API over SQLite, PostgreSQL and MariaDB | [/StormByte-Database](https://dev.stormbyte.org/StormByte-Database) |
| [Logger](https://github.com/StormBytePP/StormByte-Logger) | Stream logging, levels, headers, redaction, `ThreadedLog` | [/StormByte-Logger](https://dev.stormbyte.org/StormByte-Logger) |
| **Multimedia** | This repository | [/StormByte-Multimedia](https://dev.stormbyte.org/StormByte-Multimedia) |
| [Network](https://github.com/StormBytePP/StormByte-Network) | Framed packets, Client/Server, IPv4/IPv6 TCP and Buffer pipelines (compress/encrypt) | [/StormByte-Network](https://dev.stormbyte.org/StormByte-Network) |
| [System](https://github.com/StormBytePP/StormByte-System) | Processes, pipes and environment variables across Linux, Windows and macOS | [/StormByte-System](https://dev.stormbyte.org/StormByte-System) |

## Table of Contents

- [What this module does](#what-this-module-does)
- [The rest of the suite](#the-rest-of-the-suite)
- [Documentation](#documentation)
- [Two ways to work](#two-ways-to-work)
  - [1. Transcoder (File → File)](#1-transcoder-file--file)
  - [2. The tube by hand](#2-the-tube-by-hand)
- [Plan, items and the tube contract](#plan-items-and-the-tube-contract)
- [Filters and analytics](#filters-and-analytics)
- [Logging](#logging)
- [Build options and distribution](#build-options-and-distribution)
- [Installation](#installation)
- [Contributing](#contributing)
- [License](#license)
- [Supporting the project](#supporting-the-project)

## Documentation

- This README: how to build, the two entry points, the tube contract, distribution flags.
- Doxygen class reference (headers under `StormByte/multimedia/`): [https://dev.stormbyte.org/StormByte-Multimedia/](https://dev.stormbyte.org/StormByte-Multimedia/).
- Logger contract used by every `Step`: [https://dev.stormbyte.org/StormByte-Logger/](https://dev.stormbyte.org/StormByte-Logger/).

## Two ways to work

Every job is the same tube. You either let `Transcoder` assemble it from a fluent map of origin streams, or you construct the `Step`s yourself and join them with `operator>>`. There is no third private path.

### 1. Transcoder (File → File)

`Transcoder` is the facade most applications want. It opens a source, lets you name **output** tracks in mux order, attaches filters, picks a destination container and path, and runs the coordinator. The stock class is complete: you do not have to derive anything to remux, recode or filter.

It is also **designed to be inherited**. Override `EmptyPlan()` / `EmptySettled()` to carry your own fields, or the hooks (`OnConfigure`, `OnStart`, `OnPlan`, `OnSettled`, `OnProgress`, `OnDone`, `OnError`, `OnAborted`) to drive a UI or a batch runner. Override `InstallLog()` so this job’s own lines use another component path; tube stages stay under `StormByte/Multimedia/<stage>`. Hooks are not an escape hatch around the tube. If a hand-wired tube cannot do it, `Transcoder` will not sneak it in.

Open the source, map streams, run, poll:

```cpp
#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/multimedia/pipeline/filters/video/scale.hxx>
#include <StormByte/multimedia/pipeline/filters/video/watermark.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>

using StormByte::Logger::Level;
using StormByte::Logger::ThreadedLog;
using StormByte::Multimedia::Registry;
using StormByte::Multimedia::Pipeline::Status;
using StormByte::Multimedia::Pipeline::Transcoder;
using StormByte::Multimedia::Pipeline::Filter::Video::Anchor;
using StormByte::Multimedia::Pipeline::Filter::Video::Scale;
using StormByte::Multimedia::Pipeline::Filter::Video::Watermark;

int main(int argc, char** argv) {
	if (argc != 3) {
		std::cerr << "usage: " << argv[0] << " <in.mkv> <out.mkv>\n";
		return 1;
	}

	auto logger = std::make_shared<ThreadedLog>(std::cout, Level::Debug, "[%L] %T %c");

	auto opened = Transcoder::Open(logger, argv[1], argv[2]);
	if (!opened) {
		std::cerr << opened.error()->what() << '\n';
		return 1;
	}
	auto& job = *opened.value();

	auto& registry = Registry::Instance();
	auto hevc = registry.FindCodec("H.265");
	auto eac3 = registry.FindCodec("E-AC3");
	auto mkv  = registry.FindContainer("Matroska");
	if (!hevc || !eac3 || !mkv) {
		std::cerr << "codec or container missing in this build\n";
		return 1;
	}

	// Output order is the order of these calls. Origin index is the argument.
	job.Video(0)
		.Codec(*hevc)
		.Implementation("libx265")
		.Filter<Watermark>(logger, std::filesystem::path("/var/lib/marks/logo.png"),
			Anchor::BottomRight, 25)
		.Filter<Scale>(logger, 0u, 1080u);
	job.Audio(1).Remux();          // compressed copy, adapted to the destination
	job.Audio(2).Codec(*eac3);
	job.Subtitle(3).Remux();
	job.Attachments();             // keep attachments; omit this call to drop them

	job.Destination(*mkv, argv[2]);
	if (job.Failed()) {
		std::cerr << job.Error().value_or("configure failed") << '\n';
		return 1;
	}

	job.Run();                     // non-blocking
	for (;;) {
		const auto status = job.Status();
		if (auto pct = job.Progress())
			std::cout << "\rprogress " << *pct << "%" << std::flush;
		if (status == Status::Done)
			break;
		if (status == Status::Error || status == Status::Aborted) {
			std::cerr << '\n' << job.Error().value_or("job ended") << '\n';
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
| `Video(0).Codec(*hevc).Implementation("libx265")` | Decode origin video 0, encode HEVC with that encoder pin. |
| `.Filter<Watermark>(…)` / `.Filter<Scale>(…)` | Frame filters on that encode lane, in registration order. |
| `Audio(1).Remux()` | Keep the compressed stream. Remux is copy **plus** destination adaptation. There is no separate “Copy” stage. |
| `Audio(2).Codec(*eac3)` | Recode that origin audio. |
| `Ignore(n)` | Drop origin stream `n`. |
| `Attachments()` / `Attachments("image/png")` | Keep all attachments, or only a MIME. Default without a call is drop. |
| `Destination(container, path)` | Closes the intention. Required before `Run()`. |
| `Filter<Analytics>(…)` on the **job** | Analytics on every encode lane. Not via `Track::Filter`. |

`Run()` is asynchronous. `Pause()` / `Resume()` / `Cancel()` talk to the coordinator. After `Done`, `Reports()` holds analytics snapshots (VMAF mean/min and anything else you attached). Mux close is not analytics EOF: `Transcoder` waits for the route to go idle before `OnDone` / `Reports`.

When source duration is not supplied, `Progress` first displays only `Calculating duration` with an animated activity indicator. During source preparation and FFmpeg stream analysis there is no percentage: this work can consume CPU without advancing through the file. Once packet scanning starts, the activity indicator freezes and the line adds a monotone estimate based on processed packet positions relative to source size, not on AVIO seeks or read-ahead; formats without packet positions use accumulated packet payload bytes as an approximation. The 100 percent value is reserved for EOF followed by a successful rewind. Processing and analytics remain inactive until the scan and rewind finish; this preliminary percentage is separate from `All()`. The reader's cache and read-ahead settings are unchanged. `Progress::Snapshot()` captures the exclusive phase, optional duration/measure/analytics percentages, combined processing score, and completion flags under one lock, so applications can render their own UI without parsing the status string. During preparation its phase is `CalculatingDuration` and its `Duration` value is empty. Getters and snapshots are safe to read concurrently. Custom `EmptyPlan` factories can pass `DurationProgress()` to the observing `Plan` constructor to publish scan updates.

Capture `job.Telemetry()` before `Run()` if the final snapshot must outlive the job. It retains each stage's origin, lifecycle, frame/packet input and output counts, setup and elapsed time, Process-call total/mean/min/max, and blocked-wait total/count/max. The coordinator also logs the final multi-line report at `Info`; converting the retained handle to `std::string` produces the same report. A hand-built `Step` or filter exposes its own retained `Telemetry()` handle.

`JobTelemetry::PeakMemory()` (also `MemoryMaximum()`) is the highest sampled process resident set size in bytes; `MemoryMinimum()` and `MemoryCurrent()` report the lowest sample and last sample. The coordinator samples nominally every 20 ms and at job start/end. These values are process-wide RSS, not memory attributed to an individual stage; OS sampling may miss short-lived peaks and can be unavailable on unsupported platforms.

Quality knobs on a recode track are the obvious ones: `CRF`, `BitRate`, `MaxBitRate`, `Preset`, `Tune`, `FineTune`, plus `Language` / `Title` overrides.

### 2. The tube by hand

Same workers, no facade. You own construction, binding and lifetime. This is what you want for a custom graph (several destinations, an extra sink, a filter that is not on `Transcoder`’s fluent map — as long as it is still a `Step` / `Filter` the tube already understands).

A short recode of one video track into Matroska:

```cpp
#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/registry.hxx>

using StormByte::Logger::ThreadedLog;
using StormByte::Multimedia::Registry;
using namespace StormByte::Multimedia::Pipeline;

auto logger = std::make_shared<ThreadedLog>(std::cout, Level::Notice, "[%L] %T %c");
auto& registry = Registry::Instance();
auto hevc = registry.FindCodec("H.265");
auto mkv  = registry.FindContainer("Matroska");

File source{/* opened origin */};
Plan plan{std::move(source), *mkv, "out.mkv"};
Track video;
video.Kind = Type::Video;
video.In   = 0;
video.Codec(*hevc).Implementation("libx265");
plan.add(std::move(video));
if (auto check = plan.Check(); !check)
	return 1;

Demuxer demux(logger);
Decoder decode(logger, /* origin track */ 0);
Encoder encode(logger, /* output index */ 0, *hevc);
encode.Implementation("libx265");
Muxer   mux(logger, *mkv);

std::move(plan) >> demux;
demux >> decode >> encode >> mux >> std::filesystem::path{"out.mkv"};
```

`operator>>` shares the `Plan` and binds hoppers. `Demuxer` produces `Packet`s and receives nothing. `Decoder` turns those into `Frame`s. `Encoder` produces `Packet`s again. `Muxer` reserves the output slot — remux does not. Fan-out from one demuxer to several decoders / remuxers is the same operator.

`Filters` sits between two `shared_ptr<Step>` ends when you need a filter chain or analytics. `Transcoder` builds that graph for you. By hand:

```cpp
auto decode = std::make_shared<Decoder>(logger, 0);
auto encode = std::make_shared<Encoder>(logger, 0, *hevc);
Filters graph;
graph.Between(decode, encode).Add<Scale>(logger, 1920, 1080);
graph.Add<Filter::Video::VMAF>(logger, "vmaf_4k_v0.6.1");	// one node, every matching stretch
graph.Close();
```

## Plan, items and the tube contract

- **`Plan`** is the whole job. Move-only origin `File`. Destination container and path. `Tracks` is the list of **outputs**. `Check()` is shape, not a rehearsal of FFmpeg.
- **`Packet`** is a compressed access unit. **`Frame`** is a decoded one. No public timing setters. Mutate pixels through `Decoder` / `Encoder` / a filter `Replace`, not a setter on `Frame`.
- **`Serial`** is a monotone id assigned by the tube. Public getter, no setter. It is not a frame count.
- **`Remuxer`** forwards compressed packets and adapts them to the destination. “Copy” as a stage does not exist.
- **Caps** (`MaxCeiling`, hopper capacity) are real limits. Do not treat EOF as Fail. A filter that cannot overlay a logo disables the overlay (opacity 0, passthrough) and logs a Warning. Fail is reserved for a broken unit from the tube.
- **Content** behind `Frame` is virtual (passthrough / video / audio). After `Scale`, HDR10+ and friends are recalculated on `Replace`. Metadata is not dropped by `memcmp`.

## Filters and analytics

Filters are leaves, not a second pipeline language. `Scale` is resize (that is the name). `Watermark` is a still image on decoded video, with Hold so a black slate at the start does not pin the letterbox probe too early.

Analytics never emit into the encode lane. The last analytics node is a drain. VMAF (when built) compares a reference decode against a post-encode look: `Filters` mounts an internal decoder in EncodeLook mode, scales the distorted geometry to the latched reference, and reports mean / min against model `vmaf_4k_v0.6.1`. One libvmaf context per `Frame::Track`. Default `n_threads` is all cores; 4K 10-bit at 32 threads holds ~18.5 GiB for the job (peak ~20.5 GiB). Pass a smaller count as the third constructor argument. That look is not a user API.

Write a new filter the same way `Scale` and `Watermark` are written. Do not add public friends so a coordinator can peek.

## Logging

First argument of every `Step` and filter leaf: `std::shared_ptr<StormByte::Logger::Log>`. Prefer `ThreadedLog` if more than one thread will write.

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
| `WITH_GPL` | `ON` / `OFF` | GPL components inside bundled FFmpeg (`gpl=enabled`, `version3=enabled`). |
| `WITH_NONFREE` | `ON` / `OFF` | Nonfree components inside bundled FFmpeg. |

`WITH_GPL` and `WITH_NONFREE` change **what the bundled FFmpeg is allowed to compile**. They do not relicense StormByte-Multimedia. If you ship a binary linked against a GPL or nonfree FFmpeg, **that binary** follows FFmpeg’s license combination. Leave both `OFF` when you need a redistributable build that stays on the LGPL side of FFmpeg.

`SYSTEM` FFmpeg is whatever the host already linked; you inherit that host’s license surface.

Typical configure:

```sh
cmake -S . -B build \
  -DWITH_FFMPEG=BUNDLED \
  -DWITH_VMAF=BUNDLED \
  -DWITH_GPL=OFF \
  -DWITH_NONFREE=OFF
```

## Installation

Needs a C++26 compiler, CMake 3.12 or newer, and the StormByte modules listed above. Bundled FFmpeg also wants NASM/YASM (and Meson/Ninja via BuildMaster).

```sh
git clone --recursive https://github.com/StormBytePP/StormByte-Multimedia.git
cd StormByte-Multimedia
cmake -S . -B build
cmake --build build
```

Link `StormByte-Multimedia` (and its StormByte + FFmpeg / libvmaf deps). Include path: the public install prefix, headers as `#include <StormByte/multimedia/….hxx>`.

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

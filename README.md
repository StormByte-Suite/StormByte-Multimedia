# StormByte

Contributor guides: [Contributing](CONTRIBUTING.md) and [Coding Style](CODING_STYLE.md).

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![C++26](https://img.shields.io/badge/C%2B%2B-26-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.25+-064F8C?logo=cmake&logoColor=white)
![License: LGPL v3 / Commercial](https://img.shields.io/badge/License-LGPL_v3%20%2F%20Commercial-blue.svg)
[![CI](https://github.com/StormBytePP/StormByte-Multimedia/actions/workflows/ci.yml/badge.svg)](https://github.com/StormBytePP/StormByte-Multimedia/actions/workflows/ci.yml)
[![Sponsor](https://img.shields.io/badge/Sponsor-StormBytePP-ea4aaa?logo=githubsponsors)](https://github.com/sponsors/StormBytePP)

**StormByte-Multimedia** brings media processing and real-time preview into a C++ application without making that application manage FFmpeg's C API. The complexity stays inside the library: asynchronous workers, stream routing, decoding, filtering, encoding, muxing and presentation timing. Outside, applications describe a job or connect a player to their own display.

There are two independent workflows:

| Goal | Starting point | What the application supplies |
| --- | --- | --- |
| Copy, encode or filter media into an output | `Pipeline::Transcoder` | Input/output locations, output-track mapping and settings |
| Build a specialized processing graph | `Pipeline::Plan` and `Step` stages | The same job description, explicit connections and graph lifetime |
| Preview video, audio and subtitles in a UI | `Multimedia::SimplePlayer` | A buffered input and a consumer-implemented `Display` |

`Transcoder` is a complete facade over the processing pipeline, not a reduced encoder. Manual wiring exposes the same machinery when a graph needs more control. `SimplePlayer` handles real-time playback independently; it is a **preview service, not a replacement for a professional media player**, an editor or a bundled Qt/SDL widget.

It depends on [StormByte Base](https://github.com/StormByte-Suite/StormByte/releases/tag/2.2.2) 2.2.2, [StormByte Buffer](https://github.com/StormByte-Suite/StormByte-Buffer/releases/tag/2.0.5) 2.0.5, [StormByte Logger](https://github.com/StormByte-Suite/StormByte-Logger/releases/tag/2.0.5) 2.0.5 and [StormByte System](https://github.com/StormByte-Suite/StormByte-System/releases/tag/2.1.2) 2.1.2. Each is a release archive. Public headers live under `StormByte/multimedia/`.

The suite is split on purpose. Base, Buffer, Config, Crypto, Database, Logger, Network and System are **other repositories**. This one does not implement them.

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

- [Documentation](#documentation)
- [Inspect media and discover capabilities](#inspect-media-and-discover-capabilities)
- [Encoding and remuxing](#encoding-and-remuxing)
  - [Quick start with Transcoder](#quick-start-with-transcoder)
  - [Configure output tracks](#configure-output-tracks)
  - [Filters and analytics](#filters-and-analytics)
  - [Progress, reports and application hooks](#progress-reports-and-application-hooks)
  - [Advanced: wire the pipeline yourself](#advanced-wire-the-pipeline-yourself)
  - [Metadata, subtitles and artwork](#metadata-subtitles-and-artwork)
- [Preview playback with SimplePlayer](#preview-playback-with-simpleplayer)
  - [Open an input and control playback](#open-an-input-and-control-playback)
  - [Build a Display adapter](#build-a-display-adapter)
  - [Reader ownership and transfer to Transcoder](#reader-ownership-and-transfer-to-transcoder)
  - [Scope and limitations](#scope-and-limitations)
  - [Interactive output demo](#interactive-output-demo)
- [Shared integration rules](#shared-integration-rules)
- [Build options and distribution](#build-options-and-distribution)
- [Installation](#installation)
- [Tests](#tests)
- [Contributing](#contributing)
- [License](#license)
- [Supporting the project](#supporting-the-project)

## Documentation

This README explains the workflows, integration choices and important contracts. It is not a method-by-method API reference.

- [Doxygen reference](https://suite.stormbyte.org/StormByte-Multimedia/): public classes, overloads, filter parameters and detailed contracts.
- [Public headers](lib/public/StormByte/multimedia/): the source of those contracts.
- [Test fixtures and provenance](test/files/README.md): synthetic media, metadata fixtures and redistribution notices.
- [Logger reference](https://suite.stormbyte.org/StormByte-Logger/): logging and provider lifetime rules.

## Inspect media and discover capabilities

Use `File` to inspect streams, typed audio/video properties, metadata and attachments before presenting choices to a user. `File::Streams()` contains ordinary tracks; `File::Attachments()` exposes pictures and other attachments separately. An album cover is not an ordinary video track.

The registry provides stable codec/container identities, aliases and the implementations available in the configured build. A name being registered does not imply an encoder, decoder or suitable output container is available.

```cpp
#include <StormByte/multimedia/registry.hxx>

using namespace StormByte::Multimedia;

auto hevc = Registry::Instance().FindCodec("H.265");
if (hevc) {
	const Codec& codec = hevc->get(); // Borrowed registry identity.
	const bool writable = codec.HasAccess(Operation::Write);
	const bool hdrWritable = codec.HasAccess(Operation::Write, Feature::HDR10);
	const bool doviReadable = codec.HasAccess(Operation::Read, Feature::DOVI);
}
```

`HasAccess(Access, Features)` requires a covering available implementation in each requested direction. An empty feature mask is the generic access query. This is a compiled-capability check, not a device probe or a guarantee that every setting, profile and container combination will open.

`Codec::Encoders(features)` lists concrete implementations and their supported controls; `EncoderInfo::Option(name)` describes option types, ranges, defaults and choices. This lets a UI expose settings the selected encoder can actually apply.

`Codec::SubtitleKind()` classifies subtitle codecs as text, bitmap or unknown independently of decoder/encoder availability. ASS, SRT, WebVTT and `mov_text` are text; PGS, DVD/DVB and XSUB are bitmap. Classification does not imply OCR or rendering support.

## Encoding and remuxing

Processing creates an output, not a timed presentation. Both entry points use the same demux/decode/filter/encode/mux stages and explicit failure handling:

- **Start with `Transcoder`** for ordinary jobs and product integration.
- **Wire the stages yourself** only when the graph requires explicit routing or lifetime control.

### Quick start with Transcoder

Remux the first video stream into Matroska. The output suffix selects the container; the application does not create FFmpeg contexts or connect workers.

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
	if (job.Failed()) {
		std::cerr << job.Error().value_or(String{"configure failed"}) << '\n';
		return 1;
	}

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

### Configure output tracks

Map the streams you want, in output order, and choose `Remux()` or `Codec(...)` for each. Omitted streams are dropped. These calls append outputs: `Audio(1)` twice means two independent destination tracks, not another lookup of the first one.

For example, after constructing a job and checking the registry identities:

```cpp
// hevc/eac3 are successful Registry::FindCodec results; logger is shared.
job.Video(0)
	.Codec(hevc->get())
	.Implementation(StormByte::Safe::String{"libx265"})
	.CRF(20);
job.Audio(1).Remux();
job.Audio(1).Codec(eac3->get());
job.Subtitle(3).Remux();
job.Attachments();
// Check Failed()/Error() before Run().
```

This can preserve the compressed original audio and add an E-AC3 compatibility track alongside it. Each output has independent settings, filters and metadata overrides. Save a returned `Track` handle when further configuring that particular output.

| Choice | Meaning |
| --- | --- |
| `Remux()` | Forward compressed media with destination adaptation; no separate “Copy” stage |
| `Codec(identity)` | Decode and encode with a registered destination codec |
| `Implementation(name)` | Pin the encoder; unavailable or mismatched pins fail without fallback |
| `Implementation(ImplementationSide::Decoder, name)` | Independently pin the source decoder on an encode lane |
| `CRF`, `BitRate`, `MaxBitRate`, `Preset`, `Tune`, `FineTune` | Configure supported quality/rate/implementation options |
| `Language`, `Title` | Override metadata on that output |
| `Ignore(n)` | Remove every mapped output of origin stream `n` |
| `Attachments()` / `Attachments(mime)` | Select general attachments, otherwise dropped; automatic audio artwork is described below |
| Explicit registered `Container` constructor argument | Override output-suffix inference; the identity is borrowed |

Unsupported settings fail rather than being ignored. Audio quality-based VBR replaces bitrate when selected; without a quality or bitrate setting, the encoder default applies, not the source bitrate. `FineTune` accepts individual keys and packs them into implementation options where necessary.

Encoder preparation preserves supported representations or performs exact storage repacking, such as `yuv420p10le`/`p010le`. Depth reduction, chroma resampling and color conversion require an explicit filter. Hardware encoder preparation can negotiate devices, pools and exact transfers; it does not automatically enable hardware decoding or silently fall back after device/profile failure.

Audio encode adaptation can automatically reduce 7.1 to 5.1 for AC-3/E-AC3. MP3 requires an explicit downmix for inputs above two channels. These are **encoding policies**, not `SimplePlayer` output behavior: preview preserves decoded channels.

### Filters and analytics

Attach typed filters to an output with `Track::Filter<T>(...)`; job-level `Filter<Analytics>(...)` attaches global analysis. The facade creates the filter graph, so a simple job can use the same filters as manual wiring.

| Use | Examples and considerations |
| --- | --- |
| Geometry | `Scale`, crop and pad; registration order determines the transformation order |
| Watermark | PNG, JPEG, WebP, BMP or self-contained SVG from a path or `Safe::Binary` |
| Restoration and cadence | Denoising, sharpening, decimation and experimental two-pass `Degrain` |
| Color | Explicit conversions and tone mapping; incompatible HDR metadata is removed from SDR output |
| Audio | Typed sample-rate, layout and processing filters |
| Quality analysis | VMAF compares decoded reference and destination pictures without modifying the output |

Watermark's `designedFor` size is the video reference size, not the requested logo size. SVG is rendered at the final paint resolution; external resources and system-font discovery are disabled. Convert text to paths and embed images. LunaSVG is not a browser SVG engine; validate the artwork visually. Invalid SVG dimensions warn and pass media through without a logo.

`Degrain` measures regional grain before filtering, preserves timing/geometry and has bounded look storage. It is an experimental heuristic, not motion-compensated restoration or a guarantee of perceptual improvement. Do not stack it with another grain-denoising filter.

VMAF reports scores **and scored-frame counts** per output. Wait for `Done` and inspect report status and counts, not just the mean. Resource modes govern workers and queue budgets; process memory still depends on resolution, concurrent contexts and retained pictures. A queue limit is not a whole-job RAM limit.

To implement a filter, follow the existing `Scale`/`Watermark` classes and the Doxygen contracts. Frame filters use `Save` for a one-for-one replacement, or `Drop`/`EmitGraph`/`EmitFrame` for buffering and multiple outputs, including EOF output. Analytics cannot mutate media. Use the library's RAII frame/packet types; the libav-backed graph surface is for operations not covered by the typed wrappers. Retain a scaling context for repeated direct conversions and use it from one thread at a time.

### Progress, reports and application hooks

`Run()` is asynchronous; `Pause()` / `Resume()` / `Cancel()` control the coordinator. Success comes from `Status() == Done`, not a progress percentage or muxer EOF.

`Progress()->Snapshot()` reads the progress axes atomically:

| Axis | Purpose |
| --- | --- |
| Duration calculation | Source scan, only when neither a positive supplied duration nor a usable container hint exists |
| `Main` | Actual destination-writing percent; 100 when the trailer is written |
| `Measure` | Optional first pass for a two-pass filter |
| `Analytics` | Optional destination analysis, which can continue after main output finishes |
| `All` | Weighted overall display, below 100 until all mounted phases complete |

Show `Main` as the transcode bar, with measure/analytics separately when present. `All` is useful for a one-line overall indicator, not as the encode percentage. `File::DurationHint()` does not scan; `File::Duration()` retains full-scan behavior. Duration estimates do not limit how much media is processed. Observed scans support pause/cancel and rewind on completion or interruption.

After `Done`, `Reports()` contains analytics results. Capture `Telemetry()` before running if the final snapshot must outlive the job. It retains stage counts, setup/processing/wait times and sampled memory. Memory minimum/current/peak are **process-wide RSS**, not stage allocations; sampling can miss short peaks.

`Transcoder` is designed to be inherited, but inheritance is not required for a normal job. Override `OnProgress`, `OnDone` and `OnError` to integrate a UI, scheduler or batch runner. Override `EmptyPlan()` / `EmptySettled()` for application-owned payloads while retaining the processing machinery and provider-safe ownership.

### Advanced: wire the pipeline yourself

For a specialized graph, construct a shared `Plan`, configure its output tracks, create the stages and connect them with `operator>>`. A recode lane has this shape:

```text
                         control/setup
                 Demuxer -------------> Muxer
                    |
                  Packet
                    v
                 Decoder
                    |
                  Frame
                    v
             [filters/analytics]
                    |
                  Frame
                    v
                 Encoder
                    |
                  Packet
                    v
                  Muxer
```

A remux lane substitutes `Remuxer` for decode/filter/encode. `Muxer` reserves output slots and interleaves destination packets; it does not trust worker arrival order. Fan-out permits independent routes from the same demuxer.

The wiring sequence below assumes already-constructed compatible stages and a checked `Safe::Shared<Plan>`:

```cpp
demux >> mux;
demux >> decode;
decode >> encode;
encode >> mux;
plan >> demux; // Last: binding publishes Running and starts job setup.
```

Important differences from the facade:

1. **Configure before binding.** `Plan` owns input/output locations and output-track intentions. It is noncopyable/nonmovable; bind the shared owner, not a value. `Check()` validates formation. Container and codec support are also checked during setup/run.
2. **Finish every connection first.** Workers exist from stage construction but wait for the Plan to run. For a filter graph, use shared endpoints, register leaves, call `Filters::Close()`, then bind.
3. **Observe, do not publish, state.** Plan states are `Created`, `Running`, `Finished` and `Failed`. Preserve the first error. `Finished` requires every participating stage to complete; muxer completion alone is insufficient.
4. **Own the lifetime.** Keep stages, graph, Plan and borrowed identities alive through completion and notifications. Wait for graph `Idle()` where applicable. Stop every peer before destroying any of them and quiesce concurrent controllers; `Step::Stop()` is nonblocking, not a join barrier.
5. **Keep errors distinct from EOF.** Normal EOF drains queued media; failure/cancellation wakes blocked peers. Queue capacities bound individual queues, not total memory.

`Packet` is compressed media and `Frame` is decoded media. Their serial is a lineage identifier, not a frame count; timing is not set through public `Frame` setters. Transform through filters and supported adapters.

`Plan::Policy::EnforceFeatures` is enabled by default: plans reject encode routes without an implementation capable of keeping source HDR10, HDR10+ or Dolby Vision. Disabling it permits feature loss; it does not manufacture metadata support. Configuration, state/error and telemetry are public; private source snapshots and reader/writer accessors are not an application inspection surface.

### Metadata, subtitles and artwork

Encoding and remuxing copy the supported typed music tags; the destination chooses their native representation. This does not preserve every arbitrary ID3 frame or a byte-identical metadata block. Muxed output advertises `StormByte-Multimedia <version>` where application tags exist; technical muxer/bitstream identifiers remain truthful, and raw streams may carry no branding.

#### HDR and Dolby Vision

Feature checks distinguish static HDR10, HDR10+ dynamic metadata and Dolby Vision. They are not interchangeable with ten-bit pixels or hardware acceleration.

- HEVC Dolby Vision decoding is listed for `hevc`; AV1 for `av1`/`libdav1d`. Listed encoders are compatible builds of `libx265`, `libsvtav1` and `libaom-av1`.
- HDR10+ paths include native HEVC/AV1 and dav1d decoding, and x265/libaom encoding. VP9 metadata preservation is container-conditional through Matroska side data, not a general VP9 bitstream promise.
- Actual support depends on the selected FFmpeg/dependency build, compatible input metadata and encoder settings. An encoder cannot create a Dolby Vision grade from ordinary pixels.

Pixel transformations can make metadata stale. DOVI-only content must not acquire an invented HDR10 fallback. Validate or regenerate metadata in a mastering workflow after grading, geometry or color changes. Successful processing and metadata preservation are not visual correctness, Dolby certification or calibrated presentation.

#### Subtitle conversion

Remux compatible subtitles or select an output subtitle codec to convert them. Bitmap-to-text conversion uses OCR; the source language selects the model, and missing models fail explicitly. OCR is not translation. This belongs to the processing workflow; `SimplePlayer` instead delivers decoded cues for the application's renderer.

#### Embedded audio artwork

Audio-only outputs automatically copy supported embedded artwork, even without `Transcoder::Attachments()`. Pictures are separate from ordinary tracks and retain their PNG/JPEG/BMP bytes without recompression.

| Output | Native representation |
| --- | --- |
| MP3 | ID3 APIC pictures |
| FLAC | FLAC PICTURE blocks |
| Ogg Opus, Vorbis, FLAC and Speex | `METADATA_BLOCK_PICTURE` comments |
| M4A / audio-only MP4 | MP4 cover metadata |
| AIFF / AIFC | ID3 pictures |
| Matroska audio | Image attachments |

Multiple pictures are retained without duplication. Picture type is not retained in the public snapshot: destinations requiring it receive the first as front cover and the rest as other. Ogg picture blocks must be smaller than 16 MiB including MIME/description.

WAV, CAF, ASF/WMA, WavPack, AU and raw AAC/AC-3/E-AC-3 have no implemented native artwork-writing route here; ordinary audio output remains usable. APE is not writable by the bundled muxer. General attachments such as subtitle fonts still require explicit selection; unsupported selected attachments fail, and WebM rejects attachments.

## Preview playback with SimplePlayer

`SimplePlayer` is in `StormByte::Multimedia`, **not** `Pipeline`. It owns decoding, real-time scheduling, synchronization, playback state and track switching. The consumer owns presentation and device integration through one abstract `Display`.

This division lets Alchemist-like applications add a play button without implementing a media clock or another decoder pipeline, while keeping their existing widget toolkit and audio integration.

### Open an input and control playback

```cpp
#include <StormByte/multimedia/simple_player.hxx>
#include <StormByte/safe/pointers.hxx>

// MyDisplay derives from Multimedia::Display; logger is optional here.
auto display = StormByte::Safe::MakeShared<MyDisplay>();
StormByte::Multimedia::SimplePlayer player(display, logger);
if (!player.Open("clip.mkv"))
	ReportError(player.Error());
else if (!player.Play())
	ReportError(player.Error());
// Later: application controls call Pause(), Seek(seconds), Play() or Stop().
// Poll State()/Error() too: decode and presentation continue asynchronously.
```

The first ordinary video and audio tracks are selected initially; subtitles are opt-in. Use `Tracks()` to build selectors and pass each track's stream index to `SelectTrack(Type, index)`. Index `-1` disables that media kind. Attached pictures are not active video tracks; the UI chooses its audio-only placeholder.

Controls return `bool`; check failures and monitor `PlaybackState::Error` while playing. An accepted seek can still fail asynchronously. `Ended` means decoding **and presentation** have drained, not merely input EOF. After `Ended`, use `Seek` or `Stop` before `Play`.

`Seek` uses absolute media seconds and preserves playing/paused intent. Paused seeks reconstruct the covering image and active subtitle cues without sending audio; resuming starts from that media position. `Position()` is the playback timeline, while `Duration()` is zero when unknown. Playback and absolute seeks do not require a known duration; a UI must not turn unknown duration into an upper seek limit of zero.

### Build a Display adapter

Include `display.hxx` and derive one application class from `StormByte::Multimedia::Display`. Implement **all eight callbacks**, even for an audio-only consumer. Construct it with `Safe::MakeShared<MyDisplay>()` and pass the shared owner to the player; the dynamic type is preserved. No Qt, SDL, FFmpeg or operating-system headers/handles are required by the public player contract.

Think of the adapter as four small cooperating pieces:

1. A current-generation token and bounded, thread-safe delivery queues.
2. A video/subtitle surface on the application's GUI thread.
3. An audio device adapter with format/layout negotiation and a truthful clock.
4. Pause/flush/drain bookkeeping connecting those pieces.

The player already schedules presentation. The adapter should not build a second independent playback timeline or decode the input again.

#### Threading and invalidation come first

Callbacks run on worker or control-caller threads, **not necessarily the GUI thread**. Do not reenter any player method from a callback, including getters. Keep callbacks bounded and marshal widget work to the UI thread. Throw on delivery/device failure so the player can expose it; do not silently discard current-generation output.

`Invalidate(epoch, active_video)` synchronously publishes the new generation, flushes audio, clears cues/images and cancels pending presentation timers. Deferred GUI work must compare its epoch again **when executed**, not just when queued. Otherwise an old image or subtitle can reappear after a seek, stop, close or track switch. When `active_video` is false, draw the application's chosen black/cover/placeholder; the library does not create dummy pixels.

`Pause(paused, epoch)` freezes/resumes the audio device and subtitle timers for the current generation. Queued pause operations must also respect invalidation.

#### Deliver pictures, samples and cues

| Callback | Payload | Adapter work |
| --- | --- | --- |
| `Video(image)` | Safe-owned packed RGBA8, dimensions, media position/duration and epoch | Present or enqueue the scheduled picture; recheck epoch on GUI execution |
| `Audio(audio)` | Safe-owned **actual decoded PCM**, rate, scalar format, planarity, ordered channel identities, sample count, position and epoch | Queue without waiting for playback; negotiate device format and map channels |
| `Subtitle(cue)` | Text, ASS events/style/fonts or RGBA8 bitmap regions, coordinates, cue identity and timing | Render the appropriate representation and preserve overlapping cues |

For audio, `m_planes` contains one plane per channel when planar, otherwise one interleaved plane. Bytes are native-endian, tightly packed and exclude decoder padding. The player performs **no downmix, remapping or resampling**. Signed integer and floating formats, mono, stereo and multichannel sources remain explicit.

The adapter owns scalar conversion, interleaving, sample-rate conversion and device negotiation. Interpret `m_layout` and ordered `m_channels`, not just their count: **BL/BR and SL/SR are different speakers**. Unspecified, unknown/custom and ambisonic identities are not permission to guess a speaker map. Reject unsupported layouts or make an explicit application policy; do not silently force stereo. Reconfigure safely if the decoded format changes.

Subtitle decoding is not subtitle drawing. ASS needs styling and embedded font handling; bitmap cues need canvas placement. `Position(position, epoch)` advances the subtitle clock and expires finite cues at `m_end`. Infinite cues remain until cleared. Empty `m_regions` clears the given `m_id`, or all cues when ID is zero. Invalidation clears every old-generation cue.

#### Report what has actually played

`AudioClock(epoch)` returns current-generation `AudioPresentation`:

- `m_valid` says meaningful device feedback is available.
- The audible cursor is **`m_position - m_latency`**, in media seconds; latency is nonnegative.
- The cursor must remain frozen during pause and account for samples still unheard in device buffers.
- `m_drained` concerns audio only, not queued GUI work.

Without usable audio feedback, the player uses its steady clock, including audio gaps and video/subtitle tails. Do not report queued samples as already heard.

`Drained(epoch)` returns true only when **all** pending audio and GUI deliveries for that generation are finished. This is how the player distinguishes accepted callbacks from completed presentation at EOF.

Bound memory in the adapter too. Audio arrives at most 150 ms ahead plus one decoded block; keep enough device headroom without retaining the whole stream. Keep video and GUI queues short. The library's pending-event limit is 256 units/64 MiB, but in-flight conversion, decoder scratch, codec reference frames and your queues are additional storage.

### Reader ownership and transfer to Transcoder

`Open(std::string_view)` constructs a `BufferedFileReader`, copying the path into its `Safe::String`. `Open(Safe::Unique<Buffer::IO::BufferedLocationReader>)` takes exclusive ownership of a compatible buffered reader, preserving its subtype. It opens a not-yet-open reader once or rewinds a healthy already-open one. Failed Open retains the reader.

| Operation | Workers/presentation | Reader |
| --- | --- | --- |
| `Stop()` | Cancel/join and invalidate | Rewind the actual buffered reader; retain it open |
| `Close()` / destructor | Join and invalidate before releasing input | Close/release without rewind |
| `ReleaseReader()` | Stop/join and invalidate | Rewind, then transfer the same open dynamic reader at cursor zero |

Stop/transfer failures are explicit and retain ownership. An empty transfer result means failure, not a closed replacement reader. Destruction is noexcept and does not close the shared Display/widget.

```cpp
auto input = player.ReleaseReader();
if (!input)
	ReportError(player.Error());
else {
	StormByte::Multimedia::Pipeline::Transcoder job(
		std::move(input), destination, logger);
	// Configure output tracks, check errors, then Run().
}
```

This matters for Network's `BufferedRemoteFileReader`: closing permanently releases its mount token, so that object cannot simply reopen for transcoding. Transfer the healthy reader instead and keep its client/transport alive. The player rejects closed/faulted rewind paths rather than using Close+Open as recovery. A closed/faulted remote source requires a new reader.

### Scope and limitations

The common engine and abstract contract support **Windows, Linux and macOS**. Consumers provide their platform integration; no native output backend is bundled into Multimedia. Decode availability depends on the configured codecs. Cross-platform design is not a claim that every native backend or platform was runtime-tested.

Preview video is RGBA8 SDR-oriented conversion, not calibrated HDR/Dolby Vision output. There is no surround bitstream passthrough or automatic subtitle compositor. Subtitle seeks reconstruct cue state from the beginning, so long inputs can take time; reconstruction is bounded and fails explicitly if it exceeds storage limits.

Buffering permits local or network-backed playback without loading the whole file. Reads/probing/seeks and worker joining can wait for bounded origin I/O: configure network timeouts. Actual seek support depends on the reader and demuxer, not on having a duration value.

### Interactive output demo

The optional SDL2 demo is a real consumer-side `Display` example, not an installed library backend. Its implementation is in [test/preview_demo.cxx](test/preview_demo.cxx); SDL linkage stays outside Multimedia's public API.

```sh
cmake -S . -B build -DENABLE_PREVIEW_DEMO=ON
cmake --build build --target MultimediaPreviewDemo -j"$(nproc)"
build/test/MultimediaPreviewDemo "/path/to/video.mkv"
```

Controls: **Space** play/pause, **Left/Right** seek 30 seconds, **Home** stop/rewind, **A/V/S** cycle audio/video/subtitles including disabled, **Esc** close. It preserves image aspect ratio and shows text cues in the window title, but does not compose ASS styling or bitmap subtitles.

Use `--smoke` for three-second segments or `--listen` for thirty-second segments: initial playback, seeks to 50%, 90%, 10%, then Stop/verify zero. Unknown-duration input uses absolute targets 30, 120, 10 seconds instead.

The audio adapter requests the source rate and all channels, converts decoded PCM to device float32 and maps supported speaker sets to SDL order. It rejects unsupported/unknown/ambisonic layouts rather than falling back to stereo. SDL/hardware conversion and physical speaker routing remain external; an obtained six-channel spec is not proof of physical six-channel hardware or exact latency.

Reports include actual post-seek video/audio positions, image hashes, callback short reads, missing frames, inserted silence and sample peaks. These help diagnose delivery; clean counters do not prove audible correctness. Add a diagnostic directory after either mode to capture bounded per-epoch PCM with format/layout metadata sidecars.

## Shared integration rules

### DLL boundaries and lifetimes

Public owning data crosses modules through `StormByte::Safe` values, collections and owners. Create polymorphic owners with `Safe::MakeShared<T>(...)` or the appropriate Safe unique factory; preserve dynamic type and provider-local destruction rather than slicing. Borrowed registry identities must outlive their users.

Safe ownership is not arbitrary-ABI compatibility or a safe-unload guarantee. Consumers/providers need compatible C++/STL ABIs, and every provider supplying live values, release operations or callbacks must remain loaded until those objects are released. `STORMBYTE_DECLARE_MAYBE_SAFE` is the provider's boundary-safety promise, not an automatic audit.

Derived `Transcoder` providers create their own `Plan`/`TrackSettled` payloads in `EmptyPlan()`/`EmptySettled()` using provider-safe factories. Plans remain shared and do not implement cloning/moving; derived settled payloads must implement their required `Clone()`/`Move()` operations in the creating module.

### Logging

Stages and filters accept `Safe::Shared<Logger::Log>`; `Transcoder` requires a nonempty logger, while `SimplePlayer`'s logger is optional. Prefer `ThreadedLog` for concurrent workers and keep its underlying stream alive through every retaining job/stage.

The application controls logging levels, headers and redaction. Processing stages scope their messages under `StormByte/Multimedia/<stage>`. Detailed per-unit logging has a real cost; use it for diagnosis rather than ordinary UI progress. Observe status/error explicitly instead of treating a quiet log as success.

## Build options and distribution

| Option | Values | Meaning |
| --- | --- | --- |
| `WITH_FFMPEG` | `BUNDLED` (default) / `SYSTEM` | Bundled libav engine or compatible system installation |
| `WITH_VMAF` | `BUNDLED` / `SYSTEM` | Must match FFmpeg mode; a differing cache value is overridden with a status message |
| `WITH_OCR` | `BUNDLED` (default) / `SYSTEM` | Tesseract/Leptonica; Windows forces bundled OCR |
| `WITH_TESSDATA` | `BUNDLED` (default) / `SYSTEM` | Embedded selected models or installed runtime models, independent of OCR mode |
| `WITH_ZIMG` | `BUNDLED` / `SYSTEM` | Image-processing dependency |
| `BUILD_SHARED_LIBS` | `ON` (default) / `OFF` | Shared/static Multimedia and matching StormByte archives |
| `ENABLE_TEST` | `ON` / `OFF` (default) | Build/register CTest; PGO enables tests in the instrumented tree |
| `ENABLE_PREVIEW_DEMO` | `ON` / `OFF` (default) | Optional SDL2 output demo; requires SDL2 development files |
| `ENABLE_LTO` | `ON` (default) / `OFF` | Link-time optimization |
| `ENABLE_PGO` | `ON` / `OFF` (default) | Instrument/train in `pgo-instr`, then build the profiled library |
| `ENABLE_ASAN` | `ON` / `OFF` (default) | Debug ASan/UBSan on supported non-Windows builds; disabled for Release |
| `WITH_GPL` | `ON` / `OFF` | Permit GPL bundled components, such as x265/Rubberband; OFF also omits Multimedia Rubberband/VagueDenoiser sources and headers in system mode |
| `WITH_NONFREE` | `ON` / `OFF` | Permit nonfree bundled components such as FDK-AAC and dependent filters |

These flags select dependencies/capabilities, not the license of original Multimedia source. System installations determine their own codec availability and licensing. Review the enabled components when distributing an application.

Release defaults target x86-64-v3 on GCC/Clang x86-64, AVX2 on MSVC-compatible x86 and Apple M1 on Apple Silicon. Explicit compiler flags remain authoritative. Bundled specialized SIMD kernels retain runtime selectors without raising the global baseline; bundled x86 FFmpeg requires NASM.

Bundled tessdata embeds the models selected in [the language configuration](thirdparty/tessdata/languages/CMakeLists.txt), including available vertical variants. System models are loaded from the configured installation; use `TESSDATA_DIR`, `TESSDATA_PREFIX` or `Tessdata_ROOT` to select it and deploy the required files. Partial installations are accepted, but an absent requested language fails without fallback. OCR receives decoded grayscale subtitle pixels, not external image files.

Bundled SVG rendering uses LunaSVG 3.5.0/PlutoVG 1.3.1. Binary packages must retain `THIRD-PARTY`, `COPYING.LunaSVG` and `COPYING.FTL` alongside the existing notices.

## Installation

Requires a C++26-capable compiler/standard library, CMake with `CXX_STANDARD 26` support (3.25+) and [StormByte-BuildMaster](https://github.com/StormByte-Suite/StormByte-BuildMaster) 2.1.3 or newer. BuildMaster downloads matching Base/Buffer/Logger/System release archives; these are not submodules. Bundled media dependencies need recursive checkout and NASM on x86.

```sh
git clone --recursive https://github.com/StormBytePP/StormByte-Multimedia.git
cd StormByte-Multimedia
cmake -S . -B build \
	-DWITH_FFMPEG=BUNDLED -DWITH_VMAF=BUNDLED \
	-DWITH_GPL=OFF -DWITH_NONFREE=OFF
cmake --build build -j"$(nproc)"
cmake --install build --prefix <prefix>
```

On platforms without `nproc`, pass the detected available CPU count to `-j`.

The in-tree target is `StormByte::Multimedia`, the library is `StormByte-Multimedia`, and installed headers use `#include <StormByte/multimedia/...>`. Deploy runtime dependencies with a compatible ABI; public headers do not expose the platform or media-engine implementation dependencies.

## Tests

```sh
cmake -S . -B build-tests -DENABLE_TEST=ON \
	-DWITH_FFMPEG=BUNDLED -DWITH_VMAF=BUNDLED \
	-DWITH_OCR=BUNDLED -DWITH_TESSDATA=BUNDLED
cmake --build build-tests -j"$(nproc)"
ctest --test-dir build-tests/test --output-on-failure
```

Processing tests cover registries, `File`, facade/manual graphs, codecs, filters, remux/encode, mux policies, interleaving, OCR, artwork and HDR metadata. Outputs are reopened and decoded through library APIs; tests do not require external media command-line tools. HDR tests compare metadata and frame counts, not merely successful file creation. Hardware cases preflight device/profile support; unavailable implementations skip, but an available implementation's processing failure does not.

Preview tests use a clocked fake Display, synthetic inputs and a remote-like reader with irreversible close. They cover exact decoded audio, playback timing, pause/seek/track changes, subtitles, errors, ownership and transfer into Transcoder without another origin Open. Run the focused group with:

```sh
cmake --build build-tests --target MultimediaSimplePlayerTests -j"$(nproc)"
ctest --test-dir build-tests/test -R '^preview\.' --output-on-failure
```

See [fixture documentation](test/files/README.md) for provenance and redistribution notices. Test success is not certification of every codec/platform, visual restoration quality or long-running workload. Real-output listening uses the optional demo separately.

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

If this saved you from building a separate media pipeline inside your application, a star is the polite nod. A well-aimed issue beats a vague “it broke”. Pull requests that keep the public API focused — `Plan`, `Step`, `Transcoder`, `SimplePlayer`, `Display` and composable filters — are the ones that land.

I wrote this because the alternative was another private transcoder in every product. Maintaining that difference takes evenings.

[Sponsor StormBytePP on GitHub](https://github.com/sponsors/StormBytePP)

Use it. Break it on purpose. Tell me which sentence in this file lied.

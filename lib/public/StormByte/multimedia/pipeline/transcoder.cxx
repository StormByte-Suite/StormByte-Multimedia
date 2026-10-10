/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Multimedia.
 *
 * StormByte-Multimedia original source is dual-licensed:
 *
 * 1. GNU Lesser General Public License v3.0 (or later)
 *    You may redistribute and/or modify this file under the terms of the
 *    GNU Lesser General Public License as published by the Free Software
 *    Foundation, either version 3 of the License, or (at your option)
 *    any later version.
 *
 * 2. Commercial license
 *    Alternatively, this file may be used under the terms of a commercial
 *    license agreement with the copyright holder
 *    (David C. Manuelda <StormByte@gmail.com>).
 *
 * Both licenses apply only to original StormByte-Multimedia source in this
 * file. Third-party components — including FFmpeg and embedded trained data —
 * remain under their own licenses and are not covered by the commercial grant.
 *
 * A written StormByte commercial agreement may license this original source
 * on terms other than the LGPL, including specific use, distribution or
 * linking arrangements such as static linking, as stated in that agreement.
 * It does not grant rights to dependencies or waive their license conditions.
 * Enabling WITH_GPL or WITH_NONFREE may include components with separate
 * obligations for modification, linking (static or dynamic), redistribution
 * or works that incorporate them. The person modifying, linking, packaging or
 * distributing the resulting work is responsible for determining and meeting
 * all applicable requirements, including any needed patent permissions.
 * A StormByte commercial agreement does not provide those rights for GPL or
 * nonfree components.
 *
 * Neither license grants any patent rights. Any patent licenses required
 * to use this software or third-party components must be obtained separately
 * from the patent holders.
 *
 * StormByte-Multimedia is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with StormByte-Multimedia. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#include <StormByte/multimedia/backend/pipeline/detail/cover.hxx>
#include <StormByte/multimedia/backend/local_file_reader.hxx>
#include <StormByte/multimedia/backend/pipeline/transcoder.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>

#include <StormByte/logger/log.hxx>
#include <StormByte/multimedia/attachment.hxx>
#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/log.hxx>
#include <StormByte/multimedia/pipeline/config/attachment.hxx>
#include <StormByte/multimedia/pipeline/config/audio.hxx>
#include <StormByte/multimedia/pipeline/config/subtitle.hxx>
#include <StormByte/multimedia/pipeline/config/video.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/progress.hxx>
#include <StormByte/multimedia/stream.hxx>
#include <StormByte/safe/memory_order.hxx>
#include <StormByte/safe/unique_lock.hxx>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using StormByte::Logger::Level;
using StormByte::Multimedia::File;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	constexpr std::size_t InvalidSlot = std::numeric_limits<std::size_t>::max();

	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> LocalReader(
		const StormByte::Safe::String& path) {
		return StormByte::Multimedia::Backend::MakeLocalFileReader(path);
	}

	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> LocalWriter(
		const StormByte::Safe::String& path) {
		return StormByte::Multimedia::Backend::MakeLocalFileWriter(path);
	}

	void JobLog(const StormByte::Safe::Shared<StormByte::Logger::Log>& log,
		Level level, std::string_view text) noexcept {
		if (!log)
			return;
		*log << level << text << std::endl;
	}

	StormByte::Safe::Optional<StormByte::Multimedia::Stream> FindStream(const File& file, int index) noexcept {
		for (const auto& stream : file.Streams()) {
			if (stream.Index() == index)
				return stream;
		}
		return {};
	}

	std::string KindName(StormByte::Multimedia::Type type) noexcept {
		return ToString(type);
	}

	Config::Video* AsVideo(Config::Base* config) noexcept {
		return dynamic_cast<Config::Video*>(config);
	}

	Config::Audio* AsAudio(Config::Base* config) noexcept {
		return dynamic_cast<Config::Audio*>(config);
	}

	Config::Subtitle* AsSubtitle(Config::Base* config) noexcept {
		return dynamic_cast<Config::Subtitle*>(config);
	}

	bool AlreadyMapped(const StormByte::Safe::Vector<StormByte::Multimedia::Backend::Pipeline::TranscoderSlot>& mapped,
		int in, StormByte::Multimedia::Type kind) noexcept {
		for (const auto& slot : mapped) {
			if (slot.In == in && slot.Kind == kind)
				return true;
		}
		return false;
	}
}

TrackSettled::TrackSettled() = default;

TrackSettled::TrackSettled(const TrackSettled& other) = default;

TrackSettled::TrackSettled(TrackSettled&& other) noexcept = default;

TrackSettled::~TrackSettled() noexcept = default;

TrackSettled& TrackSettled::operator=(const TrackSettled& other) = default;

TrackSettled& TrackSettled::operator=(TrackSettled&& other) noexcept = default;

TrackSettled::PointerType TrackSettled::Clone() const {
	return MakePointer<TrackSettled>(*this);
}

TrackSettled::PointerType TrackSettled::Move() {
	return MakePointer<TrackSettled>(std::move(*this));
}

StormByte::Safe::String TrackSettled::ToString() const {
	std::string text = "settled in=" + std::to_string(In)
		+ " out=" + std::to_string(Out);
	text += Destination ? " encode" : " remux";
	if (Source)
		text += std::string(" src=") + std::string(Source->Name());
	if (Destination)
		text += std::string(" dst=") + std::string(Destination->Name());
	if (Implementation)
		text += " impl=" + static_cast<std::string>(Implementation.value());
	return StormByte::Safe::String{std::string_view{text}};
}

Transcoder::Track::Track(Transcoder& owner, StormByte::Size slot) noexcept
: m_owner(&owner), m_slot(slot) {}

Transcoder::Track& Transcoder::Track::Remux() noexcept {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	JobLog(m_owner->m_logger, Level::Debug, "track "
		+ std::to_string(m_owner->m_backend->Mapped[m_slot].In) + " marked remux");
	return *this;
}

Transcoder::Track& Transcoder::Track::Codec(const StormByte::Multimedia::Codec& codec) noexcept {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	auto& slot = m_owner->m_backend->Mapped[m_slot];
	if (codec.Type() != slot.Kind) {
		m_owner->Fail("codec '" + std::string(codec.Name()) + "' is "
			+ KindName(codec.Type()) + ", track " + std::to_string(slot.In)
			+ " is " + KindName(slot.Kind));
		return *this;
	}

	if (auto* video = AsVideo(slot.Config.get()))
		video->Codec(codec);
	else if (auto* audio = AsAudio(slot.Config.get()))
		audio->Codec(codec);
	else if (auto* subtitle = AsSubtitle(slot.Config.get()))
		subtitle->Codec(codec);
	JobLog(m_owner->m_logger, Level::Debug, "track " + std::to_string(slot.In)
		+ " encode to " + std::string(codec.Name()));
	return *this;
}

Transcoder::Track& Transcoder::Track::Implementation(StormByte::Safe::String name) {
	return Implementation(ImplementationSide::Encoder, std::move(name));
}

Transcoder::Track& Transcoder::Track::Implementation(ImplementationSide side, StormByte::Safe::String name) {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	auto& slot = m_owner->m_backend->Mapped[m_slot];
	auto impl = slot.Config->Implementation();
	StormByte::Safe::Optional<StormByte::Safe::String>* pin = nullptr;
	switch (side) {
		case ImplementationSide::Decoder: pin = &impl.Decoder; break;
		case ImplementationSide::Encoder: pin = &impl.Encoder; break;
		default:
			m_owner->Fail("implementation side is invalid");
			return *this;
	}
	if (name.empty())
		pin->reset();
	else
		*pin = std::move(name);
	slot.Config->Implementation(std::move(impl));
	return *this;
}

Transcoder::Track& Transcoder::Track::CRF(int value) {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	if (auto* video = AsVideo(m_owner->m_backend->Mapped[m_slot].Config.get()))
		video->CRF(value);
	return *this;
}

Transcoder::Track& Transcoder::Track::BitRate(std::int64_t bits_per_second) {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	auto* config = m_owner->m_backend->Mapped[m_slot].Config.get();
	if (auto* video = AsVideo(config))
		video->BitRate(bits_per_second);
	else if (auto* audio = AsAudio(config))
		audio->BitRate(bits_per_second);
	return *this;
}

Transcoder::Track& Transcoder::Track::MaxBitRate(std::int64_t bits_per_second) {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	if (auto* audio = AsAudio(m_owner->m_backend->Mapped[m_slot].Config.get()))
		audio->MaxBitRate(bits_per_second);
	return *this;
}

Transcoder::Track& Transcoder::Track::Preset(StormByte::Safe::String name) {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	auto* config = m_owner->m_backend->Mapped[m_slot].Config.get();
	if (auto* video = AsVideo(config))
		video->Preset(std::move(name));
	else if (auto* audio = AsAudio(config))
		audio->Preset(std::move(name));
	return *this;
}

Transcoder::Track& Transcoder::Track::Tune(StormByte::Safe::String name) {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	if (auto* video = AsVideo(m_owner->m_backend->Mapped[m_slot].Config.get()))
		video->Tune(std::move(name));
	return *this;
}

Transcoder::Track& Transcoder::Track::FineTune(StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options) noexcept {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	if (auto* video = AsVideo(m_owner->m_backend->Mapped[m_slot].Config.get()))
		video->FineTune(std::move(options));
	return *this;
}

Transcoder::Track& Transcoder::Track::Language(StormByte::Safe::String language) {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	m_owner->m_backend->Mapped[m_slot].Config->Language(std::move(language));
	return *this;
}

Transcoder::Track& Transcoder::Track::Title(StormByte::Safe::String title) {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	m_owner->m_backend->Mapped[m_slot].Config->Title(std::move(title));
	return *this;
}

Transcoder::Transcoder(const StormByte::Safe::String& source,
	const StormByte::Safe::String& destination,
	StormByte::Safe::Shared<StormByte::Logger::Log> logger,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
: Transcoder(LocalReader(source), LocalWriter(destination), std::move(logger), std::move(duration)) {}

Transcoder::Transcoder(const StormByte::Safe::String& source,
	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
	StormByte::Safe::Shared<StormByte::Logger::Log> logger,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
	: Transcoder(LocalReader(source), std::move(writer), std::move(logger), std::move(duration)) {}

Transcoder::Transcoder(
	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
	const StormByte::Safe::String& destination,
	StormByte::Safe::Shared<StormByte::Logger::Log> logger,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
	: Transcoder(std::move(reader), LocalWriter(destination), std::move(logger), std::move(duration)) {}

Transcoder::Transcoder(
	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
	StormByte::Safe::Shared<StormByte::Logger::Log> logger,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
: m_app_log(logger), m_logger(std::move(logger)),
	m_input_telemetry(reader ? reader->Telemetry() : StormByte::Safe::Shared<StormByte::Buffer::ReadTelemetry>{}),
	m_output_telemetry(writer ? writer->Telemetry() : StormByte::Safe::Shared<StormByte::Buffer::WriteTelemetry>{}),
	m_reader(std::move(reader)), m_writer(std::move(writer)),
	m_duration(duration && duration.value() > 0 ? std::move(duration) : StormByte::Safe::Optional<std::int64_t>{}),
	m_backend(StormByte::Safe::Unique<Backend::Pipeline::Transcoder>::MakePointer<Backend::Pipeline::Transcoder>()),
	m_armed(false) {
	InstallLog();
	if (!m_app_log)
		Fail("logger is required");
	else if (!m_reader || !m_writer)
		Fail("reader or writer is empty");
	else
		static_cast<void>(ProbeSource());
}

Transcoder::~Transcoder() noexcept {
	JobLog(m_logger, Level::LowLevel, "destroy");
	if (!m_backend)
		return;
	const auto status = m_backend->Status.load(StormByte::Safe::MemoryOrder::Acquire);
	if (status == Status::Running || status == Status::Paused)
		Cancel();
	m_backend->Join();
}

void Transcoder::InstallLog() noexcept {
	m_logger = StormByte::Multimedia::UseLog(m_app_log, "Transcoder");
	JobLog(m_logger, Level::LowLevel, "created");
}

void Transcoder::Fail(std::string_view reason) noexcept {
	if (!m_backend)
		return;
	StormByte::Safe::UniqueLock lock(m_backend->Lock);
	if (m_backend->Status.load(StormByte::Safe::MemoryOrder::Relaxed) == Status::Error)
		return;
	m_backend->Error = StormByte::Safe::String{reason};
	m_backend->Status.store(Status::Error, StormByte::Safe::MemoryOrder::Release);
	m_backend->RequestCancel();
	JobLog(m_logger, Level::Error, m_backend->Error.value());
}

bool Transcoder::ValidSlot(StormByte::Size slot) const noexcept {
	return m_backend && slot < m_backend->Mapped.size();
}

void Transcoder::AttachFilter(StormByte::Size slot, StormByte::Safe::Shared<Filter::FFmpeg> filter) noexcept {
	if (!ValidSlot(slot) || !filter)
		return;
	m_backend->Mapped[slot].Filters.push_back(std::move(filter));
}

void Transcoder::AttachAnalytics(StormByte::Safe::Shared<Filter::FFmpeg> filter) noexcept {
	if (!m_backend || !filter)
		return;
	m_backend->Analytics.push_back(std::move(filter));
}

bool Transcoder::ProbeSource() noexcept {
	if (!m_reader) {
		Fail("reader is empty");
		return false;
	}
	const auto path = m_reader->Path();
	if (path.empty()) {
		Fail("reader path is empty");
		return false;
	}
	auto opened = File::Open(*m_reader, m_duration);
	if (!opened) {
		const char* text = opened.error() ? opened.error()->what() : "file open failed";
		Fail(text);
		return false;
	}
	m_consult = StormByte::Safe::Unique<File>::MakePointer<File>(std::move(*opened));
	JobLog(m_logger, Level::Notice, std::format("probed source {}", std::string_view{path}));
	return true;
}

const StormByte::Safe::Shared<StormByte::Logger::Log>& Transcoder::Logger() const noexcept {
	return m_logger;
}

Transcoder::Track Transcoder::AddTrack(int in, Type kind) noexcept {
	JobLog(m_logger, Level::LowLevel, "add-track in=" + std::to_string(in));
	if (!m_consult && !ProbeSource())
		return Track(*this, InvalidSlot);
	if (in < 0) {
		Fail("origin index is negative");
		return Track(*this, InvalidSlot);
	}

	StormByte::Safe::Optional<Stream> stream;
	if (kind != Type::Attachment) {
		stream = FindStream(*m_consult, in);
		if (!stream) {
			Fail("source stream " + std::to_string(in) + " does not exist");
			return Track(*this, InvalidSlot);
		}
		if (stream->Type() != kind) {
			Fail("stream " + std::to_string(in) + " is " + KindName(stream->Type())
				+ ", " + KindName(kind) + "() requires " + KindName(kind));
			return Track(*this, InvalidSlot);
		}
	}
	else {
		const auto& attachments = m_consult->Attachments();
		if (static_cast<std::size_t>(in) >= attachments.size()) {
			Fail("attachment slot " + std::to_string(in) + " does not exist");
			return Track(*this, InvalidSlot);
		}
	}

	if (AlreadyMapped(m_backend->Mapped, in, kind)) {
		Fail("origin " + std::to_string(in) + " is already mapped");
		return Track(*this, InvalidSlot);
	}

	Backend::Pipeline::TranscoderSlot slot;
	slot.In = in;
	slot.Out = static_cast<int>(m_backend->Mapped.size());
	slot.Kind = kind;
	if (stream)
		slot.Source = &stream->Codec();
	if (kind == Type::Video)
		slot.Config = StormByte::Safe::Unique<Config::Base>::MakePointer<Config::Video>();
	else if (kind == Type::Audio)
		slot.Config = StormByte::Safe::Unique<Config::Base>::MakePointer<Config::Audio>();
	else if (kind == Type::Subtitle)
		slot.Config = StormByte::Safe::Unique<Config::Base>::MakePointer<Config::Subtitle>();
	else {
		const Attachment attachment = m_consult->Attachments()[static_cast<std::size_t>(in)];
		const auto& mime = attachment.MimeType();
		if (!mime || mime->empty()) {
			Fail("attachment slot " + std::to_string(in) + " has no MIME");
			return Track(*this, InvalidSlot);
		}
		slot.Config = StormByte::Safe::Unique<Config::Base>::MakePointer<Config::Attachment>(*mime);
	}

	m_backend->Mapped.push_back(std::move(slot));
	JobLog(m_logger, Level::Debug, "mapped " + KindName(kind) + " " + std::to_string(in)
		+ " -> order " + std::to_string(m_backend->Mapped.size() - 1));
	return Track(*this, m_backend->Mapped.size() - 1);
}

Transcoder::Track Transcoder::Video(int in) noexcept {
	return AddTrack(in, Type::Video);
}

Transcoder::Track Transcoder::Audio(int in) noexcept {
	return AddTrack(in, Type::Audio);
}

Transcoder::Track Transcoder::Subtitle(int in) noexcept {
	return AddTrack(in, Type::Subtitle);
}

Transcoder& Transcoder::Attachments() noexcept {
	return Attachments("*/*");
}

Transcoder& Transcoder::Attachments(std::string_view pattern) noexcept {
	if (!StormByte::Multimedia::Detail::MimePatternOk(pattern)) {
		Fail("attachment MIME pattern is not exact, type-star or star-star");
		return *this;
	}
	if (!m_consult && !ProbeSource())
		return *this;

	const auto& attachments = m_consult->Attachments();
	for (int i = 0; i < static_cast<int>(attachments.size()); ++i) {
		const Attachment attachment = attachments[static_cast<std::size_t>(i)];
		const auto& have = attachment.MimeType();
		if (have && StormByte::Multimedia::Detail::MimeMatches(*have, pattern))
			AddTrack(i, Type::Attachment);
	}
	return *this;
}

Transcoder& Transcoder::Ignore(int in) noexcept {
	JobLog(m_logger, Level::LowLevel, "ignore " + std::to_string(in));
	if (!m_consult && !ProbeSource())
		return *this;
	if (!FindStream(*m_consult, in)) {
		Fail("source stream " + std::to_string(in) + " does not exist");
		return *this;
	}

	auto& mapped = m_backend->Mapped;
	mapped.erase(std::remove_if(mapped.begin(), mapped.end(),
		[in](const Backend::Pipeline::TranscoderSlot& slot) { return slot.In == in; }),
		mapped.end());
	for (int i = 0; i < static_cast<int>(mapped.size()); ++i)
		mapped[static_cast<std::size_t>(i)].Out = i;
	JobLog(m_logger, Level::Debug, "ignore stream " + std::to_string(in));
	return *this;
}

void Transcoder::Run() noexcept {
	if (!m_backend)
		return;
	const auto current = m_backend->Status.load(StormByte::Safe::MemoryOrder::Acquire);
	if (current == Status::Error)
		return;
	if (current == Status::Running || current == Status::Paused || m_armed) {
		Fail("Run was already called");
		return;
	}
	m_backend->Start(*this);
}

void Transcoder::Cancel() noexcept {
	if (m_backend)
		m_backend->RequestCancel();
}

void Transcoder::Pause() noexcept {
	if (!m_backend)
		return;
	if (m_backend->Status.load(StormByte::Safe::MemoryOrder::Acquire) != Status::Running)
		return;
	m_backend->Paused.store(true, StormByte::Safe::MemoryOrder::Release);
	m_backend->Status.store(Status::Paused, StormByte::Safe::MemoryOrder::Release);
	JobLog(m_logger, Level::Notice, "paused");
}

void Transcoder::Resume() noexcept {
	if (!m_backend)
		return;
	if (m_backend->Status.load(StormByte::Safe::MemoryOrder::Acquire) != Status::Paused)
		return;
	m_backend->Paused.store(false, StormByte::Safe::MemoryOrder::Release);
	m_backend->Status.store(Status::Running, StormByte::Safe::MemoryOrder::Release);
	m_backend->PauseCv.notify_all();
	JobLog(m_logger, Level::Notice, "resumed");
}

enum Status Transcoder::Status() const noexcept {
	if (!m_backend)
		return Status::Error;
	return m_backend->Status.load(StormByte::Safe::MemoryOrder::Acquire);
}

bool Transcoder::Failed() const noexcept {
	return Status() == Status::Error;
}

StormByte::Safe::Optional<StormByte::Safe::String> Transcoder::Error() const noexcept {
	if (!m_backend)
		return {};
	StormByte::Safe::UniqueLock lock(m_backend->Lock);
	return m_backend->Error;
}

StormByte::Safe::Shared<StormByte::Buffer::ReadTelemetry> Transcoder::InputTelemetry() const noexcept {
	return m_input_telemetry;
}

StormByte::Safe::Shared<StormByte::Buffer::WriteTelemetry> Transcoder::OutputTelemetry() const noexcept {
	return m_output_telemetry;
}

StormByte::Safe::Shared<const class Progress> Transcoder::Progress() const noexcept {
	if (!m_backend)
		return {};
	StormByte::Safe::UniqueLock lock(m_backend->Lock);
	return m_backend->Clock;
}

StormByte::Safe::Vector<StormByte::Safe::Pair<StormByte::Safe::String, Filter::Report>> Transcoder::Reports() const noexcept {
	if (!m_backend)
		return {};
	return m_backend->Reports;
}

StormByte::Safe::Shared<const JobTelemetry> Transcoder::Telemetry() const noexcept {
	if (!m_backend)
		return {};
	return m_backend->Metrics;
}

Transcoder::operator bool() const noexcept {
	const auto status = Status();
	return status != Status::Error && status != Status::Aborted;
}

StormByte::Safe::Shared<class Plan> Transcoder::EmptyPlan(
	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
	StormByte::Safe::Optional<std::int64_t> duration) const noexcept {
	const auto progress = DurationProgress();
	return StormByte::Safe::Shared<class Plan>::MakePointer<class Plan>(std::move(reader), std::move(writer), std::move(duration),
		&progress);
}

StormByte::Safe::Function<void(double)> Transcoder::DurationProgress() const noexcept {
	auto context = std::make_unique<const Transcoder*>(this);
	StormByte::Safe::Function<void(double)> observer(context.get(),
		[](void* state, double percent) {
			const auto* owner = *static_cast<const Transcoder**>(state);
			if (auto clock = StormByte::Safe::ConstPointerCast<class Progress>(owner->Progress())) {
				clock->SetDurationCalculation(percent);
				const_cast<Transcoder*>(owner)->OnProgress();
			}
			return StormByte::Safe::Status::Success;
		},
		[](const void* state) noexcept -> void* {
			try {
				return std::make_unique<const Transcoder*>(*static_cast<const Transcoder* const*>(state)).release();
			} catch (...) {
				return nullptr;
			}
		},
		[](void* state) noexcept {
			const std::unique_ptr<const Transcoder*> owner(static_cast<const Transcoder**>(state));
		});
	static_cast<void>(context.release());
	return observer;
}

StormByte::Safe::Unique<TrackSettled> Transcoder::EmptySettled() const noexcept {
	return StormByte::Safe::Unique<TrackSettled>::MakePointer<TrackSettled>();
}

void Transcoder::OnConfigure() noexcept {}

enum Status Transcoder::OnStart() noexcept {
	return Status::Running;
}

void Transcoder::OnPlan(const class Plan&) noexcept {}

void Transcoder::OnSettled(const TrackSettled& row) noexcept {
	JobLog(m_logger, Level::Debug, row.ToString());
}

void Transcoder::OnMeasureDone() noexcept {}

void Transcoder::OnAnalyticsDone() noexcept {}

void Transcoder::OnProgress() noexcept {}

void Transcoder::OnDone() noexcept {}

void Transcoder::OnError(const StormByte::Safe::String&) noexcept {}

void Transcoder::OnAborted() noexcept {}

void Transcoder::MarkSettled(int in, Encoder& encoder) noexcept {
	if (!m_backend)
		return;
	for (auto& slot : m_backend->Mapped) {
		if (slot.In != in || slot.Settled)
			continue;
		slot.Settled = true;
		auto row = EmptySettled();
		if (!row) {
			Fail("EmptySettled returned an empty owner");
			return;
		}
		row->In = slot.In;
		row->Out = slot.Out;
		row->Kind = slot.Kind;
		row->Source = slot.Source;
		if (const auto* video = AsVideo(slot.Config.get()))
			row->Destination = video->Codec();
		else if (const auto* audio = AsAudio(slot.Config.get()))
			row->Destination = audio->Codec();
		else if (const auto* subtitle = AsSubtitle(slot.Config.get()))
			row->Destination = subtitle->Codec();
		if (encoder.Implementation())
			row->Implementation = StormByte::Safe::String{std::string_view{*encoder.Implementation()}};
		if (encoder.CRF())
			row->Crf = *encoder.CRF();
		if (encoder.BitRate())
			row->BitRate = *encoder.BitRate();
		if (encoder.MaxBitRate())
			row->MaxBitRate = *encoder.MaxBitRate();
		if (encoder.Preset())
			row->Preset = StormByte::Safe::String{std::string_view{*encoder.Preset()}};
		if (encoder.Tune())
			row->Tune = StormByte::Safe::String{std::string_view{*encoder.Tune()}};
		for (const auto& [key, value] : encoder.FineTune())
			row->FineTune[StormByte::Safe::String{std::string_view{key}}] = StormByte::Safe::String{std::string_view{value}};
		if (encoder.AudioSampleFormat())
			row->SampleFormat = *encoder.AudioSampleFormat();
		if (encoder.AudioChannels())
			row->EncoderChannels = *encoder.AudioChannels();
		if (encoder.AudioFrameSize())
			row->FrameSize = *encoder.AudioFrameSize();
		if (encoder.AudioSampleRate())
			row->SampleRate = *encoder.AudioSampleRate();
		OnSettled(*row);
		return;
	}
}

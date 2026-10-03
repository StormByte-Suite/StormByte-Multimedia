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
#include <StormByte/safe/wstring.hxx>

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using StormByte::Logger::Level;
using StormByte::Multimedia::File;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	constexpr std::size_t InvalidSlot = std::numeric_limits<std::size_t>::max();

	StormByte::Safe::String LocationText(const std::filesystem::path& path) {
		const auto native = path.wstring();
		return StormByte::Safe::String{StormByte::Safe::WString{std::wstring_view{native}}};
	}

	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> LocalReader(
		const std::filesystem::path& path) {
		return StormByte::Multimedia::Backend::MakeLocalFileReader(LocationText(path));
	}

	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> LocalWriter(
		const std::filesystem::path& path) {
		return StormByte::Multimedia::Backend::MakeLocalFileWriter(LocationText(path));
	}

	void JobLog(const std::shared_ptr<StormByte::Logger::Log>& log,
		Level level, std::string_view text) noexcept {
		if (!log)
			return;
		*log << level << text << std::endl;
	}

	const StormByte::Multimedia::Stream* FindStream(const File& file, int index) noexcept {
		for (const auto& stream : file.Streams()) {
			if (stream.Index() == index)
				return &stream;
		}
		return nullptr;
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

	bool AlreadyMapped(const std::vector<StormByte::Multimedia::Backend::Pipeline::TranscoderSlot>& mapped,
		int in, StormByte::Multimedia::Type kind) noexcept {
		for (const auto& slot : mapped) {
			if (slot.In == in && slot.Kind == kind)
				return true;
		}
		return false;
	}
}

std::string TrackSettled::ToString() const {
	std::string text = "settled in=" + std::to_string(In)
		+ " out=" + std::to_string(Out);
	text += Destination ? " encode" : " remux";
	if (Source)
		text += std::string(" src=") + std::string(Source->Name());
	if (Destination)
		text += std::string(" dst=") + std::string(Destination->Name());
	if (Implementation)
		text += " impl=" + *Implementation;
	return text;
}

Transcoder::Track::Track(Transcoder& owner, std::size_t slot) noexcept
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

Transcoder::Track& Transcoder::Track::Implementation(std::string name) noexcept {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	auto& slot = m_owner->m_backend->Mapped[m_slot];
	auto impl = slot.Config->Implementation();
	impl.Encoder = std::move(name);
	slot.Config->Implementation(std::move(impl));
	return *this;
}

Transcoder::Track& Transcoder::Track::CRF(int value) noexcept {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	if (auto* video = AsVideo(m_owner->m_backend->Mapped[m_slot].Config.get()))
		video->CRF(value);
	return *this;
}

Transcoder::Track& Transcoder::Track::BitRate(std::int64_t bits_per_second) noexcept {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	auto* config = m_owner->m_backend->Mapped[m_slot].Config.get();
	if (auto* video = AsVideo(config))
		video->BitRate(bits_per_second);
	else if (auto* audio = AsAudio(config))
		audio->BitRate(bits_per_second);
	return *this;
}

Transcoder::Track& Transcoder::Track::MaxBitRate(std::int64_t bits_per_second) noexcept {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	if (auto* audio = AsAudio(m_owner->m_backend->Mapped[m_slot].Config.get()))
		audio->MaxBitRate(bits_per_second);
	return *this;
}

Transcoder::Track& Transcoder::Track::Preset(std::string name) noexcept {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	auto* config = m_owner->m_backend->Mapped[m_slot].Config.get();
	if (auto* video = AsVideo(config))
		video->Preset(std::move(name));
	else if (auto* audio = AsAudio(config))
		audio->Preset(std::move(name));
	return *this;
}

Transcoder::Track& Transcoder::Track::Tune(std::string name) noexcept {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	if (auto* video = AsVideo(m_owner->m_backend->Mapped[m_slot].Config.get()))
		video->Tune(std::move(name));
	return *this;
}

Transcoder::Track& Transcoder::Track::FineTune(std::map<std::string, std::string> options) noexcept {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	if (auto* video = AsVideo(m_owner->m_backend->Mapped[m_slot].Config.get()))
		video->FineTune(std::move(options));
	return *this;
}

Transcoder::Track& Transcoder::Track::Language(std::string language) noexcept {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	m_owner->m_backend->Mapped[m_slot].Config->Language(std::move(language));
	return *this;
}

Transcoder::Track& Transcoder::Track::Title(std::string title) noexcept {
	if (!m_owner || !m_owner->ValidSlot(m_slot))
		return *this;
	m_owner->m_backend->Mapped[m_slot].Config->Title(std::move(title));
	return *this;
}

Transcoder::Transcoder(const std::filesystem::path& source,
	const std::filesystem::path& destination,
	std::shared_ptr<StormByte::Logger::Log> logger,
	std::optional<std::chrono::nanoseconds> duration) noexcept
: Transcoder(LocalReader(source), LocalWriter(destination), std::move(logger), std::move(duration)) {}

Transcoder::Transcoder(const std::filesystem::path& source,
	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
	std::shared_ptr<StormByte::Logger::Log> logger,
	std::optional<std::chrono::nanoseconds> duration) noexcept
	: Transcoder(LocalReader(source), std::move(writer), std::move(logger), std::move(duration)) {}

Transcoder::Transcoder(
	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
	const std::filesystem::path& destination,
	std::shared_ptr<StormByte::Logger::Log> logger,
	std::optional<std::chrono::nanoseconds> duration) noexcept
	: Transcoder(std::move(reader), LocalWriter(destination), std::move(logger), std::move(duration)) {}

Transcoder::Transcoder(
	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
	std::shared_ptr<StormByte::Logger::Log> logger,
	std::optional<std::chrono::nanoseconds> duration) noexcept
: m_app_log(logger), m_logger(std::move(logger)),
	m_input_telemetry(reader ? reader->Telemetry() : StormByte::Safe::Shared<StormByte::Buffer::ReadTelemetry>{}),
	m_output_telemetry(writer ? writer->Telemetry() : StormByte::Safe::Shared<StormByte::Buffer::WriteTelemetry>{}),
	m_reader(std::move(reader)), m_writer(std::move(writer)),
	m_duration(duration && duration->count() > 0 ? duration : std::nullopt),
	m_backend(std::make_unique<Backend::Pipeline::Transcoder>()),
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
	const auto status = m_backend->Status.load(std::memory_order_acquire);
	if (status == Status::Running || status == Status::Paused)
		Cancel();
	m_backend->Join();
}

void Transcoder::InstallLog() noexcept {
	m_logger = StormByte::Multimedia::UseLog(m_app_log, "Transcoder");
	JobLog(m_logger, Level::LowLevel, "created");
}

void Transcoder::Fail(std::string reason) noexcept {
	if (!m_backend)
		return;
	std::lock_guard lock(m_backend->Lock);
	if (m_backend->Status.load(std::memory_order_relaxed) == Status::Error)
		return;
	m_backend->Error = std::move(reason);
	m_backend->Status.store(Status::Error, std::memory_order_release);
	m_backend->RequestCancel();
	JobLog(m_logger, Level::Error, *m_backend->Error);
}

bool Transcoder::ValidSlot(std::size_t slot) const noexcept {
	return m_backend && slot < m_backend->Mapped.size();
}

void Transcoder::AttachFilter(std::size_t slot, std::shared_ptr<Filter::FFmpeg> filter) noexcept {
	if (!ValidSlot(slot) || !filter)
		return;
	m_backend->Mapped[slot].Filters.push_back(std::move(filter));
}

void Transcoder::AttachAnalytics(std::shared_ptr<Filter::FFmpeg> filter) noexcept {
	if (!m_backend || !filter)
		return;
	if (dynamic_cast<Filter::Analytics*>(filter.get()) == nullptr) {
		Fail("Track Process/Packet filters attach on Track::Filter");
		return;
	}
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
	m_consult = std::make_unique<File>(std::move(*opened));
	JobLog(m_logger, Level::Notice, std::format("probed source {}", std::string_view{path}));
	return true;
}

const std::shared_ptr<StormByte::Logger::Log>& Transcoder::Logger() const noexcept {
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

	const Stream* stream = nullptr;
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
		const auto& mime = m_consult->Attachments()[static_cast<std::size_t>(in)].MimeType();
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
	if (!Detail::MimePatternOk(pattern)) {
		Fail("attachment MIME pattern is not exact, type-star or star-star");
		return *this;
	}
	if (!m_consult && !ProbeSource())
		return *this;

	const auto& attachments = m_consult->Attachments();
	for (int i = 0; i < static_cast<int>(attachments.size()); ++i) {
		const auto& have = attachments[static_cast<std::size_t>(i)].MimeType();
		if (have && Detail::MimeMatches(*have, pattern))
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
	const auto current = m_backend->Status.load(std::memory_order_acquire);
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
	if (m_backend->Status.load(std::memory_order_acquire) != Status::Running)
		return;
	m_backend->Paused.store(true, std::memory_order_release);
	m_backend->Status.store(Status::Paused, std::memory_order_release);
	JobLog(m_logger, Level::Notice, "paused");
}

void Transcoder::Resume() noexcept {
	if (!m_backend)
		return;
	if (m_backend->Status.load(std::memory_order_acquire) != Status::Paused)
		return;
	m_backend->Paused.store(false, std::memory_order_release);
	m_backend->Status.store(Status::Running, std::memory_order_release);
	m_backend->PauseCv.notify_all();
	JobLog(m_logger, Level::Notice, "resumed");
}

enum Status Transcoder::Status() const noexcept {
	if (!m_backend)
		return Status::Error;
	return m_backend->Status.load(std::memory_order_acquire);
}

bool Transcoder::Failed() const noexcept {
	return Status() == Status::Error;
}

std::optional<std::string> Transcoder::Error() const noexcept {
	if (!m_backend)
		return std::nullopt;
	std::lock_guard lock(m_backend->Lock);
	return m_backend->Error;
}

StormByte::Safe::Shared<StormByte::Buffer::ReadTelemetry> Transcoder::InputTelemetry() const noexcept {
	return m_input_telemetry;
}

StormByte::Safe::Shared<StormByte::Buffer::WriteTelemetry> Transcoder::OutputTelemetry() const noexcept {
	return m_output_telemetry;
}

Progress::Pointer Transcoder::Progress() const noexcept {
	if (!m_backend)
		return {};
	return m_backend->Clock;
}

std::vector<std::pair<std::string, Filter::Report>> Transcoder::Reports() const noexcept {
	if (!m_backend)
		return {};
	return m_backend->Reports;
}

Transcoder::operator bool() const noexcept {
	const auto status = Status();
	return status != Status::Error && status != Status::Aborted;
}

std::unique_ptr<class Plan> Transcoder::EmptyPlan(
	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
	StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
	std::optional<std::chrono::nanoseconds> duration) const noexcept {
	return std::make_unique<class Plan>(std::move(reader), std::move(writer), std::move(duration));
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
void Transcoder::OnError(const std::string&) noexcept {}
void Transcoder::OnAborted() noexcept {}

void Transcoder::MarkSettled(int in, Encoder& encoder) noexcept {
	if (!m_backend)
		return;
	for (auto& slot : m_backend->Mapped) {
		if (slot.In != in || slot.Settled)
			continue;
		slot.Settled = true;
		auto row = EmptySettled();
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
		row->Implementation = encoder.Implementation();
		row->Crf = encoder.CRF();
		row->BitRate = encoder.BitRate();
		row->MaxBitRate = encoder.MaxBitRate();
		row->Preset = encoder.Preset();
		row->Tune = encoder.Tune();
		row->FineTune = encoder.FineTune();
		row->SampleFormat = encoder.AudioSampleFormat();
		row->EncoderChannels = encoder.AudioChannels();
		row->FrameSize = encoder.AudioFrameSize();
		row->SampleRate = encoder.AudioSampleRate();
		OnSettled(*row);
		return;
	}
}

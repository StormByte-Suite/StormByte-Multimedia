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

#include <StormByte/multimedia/backend/pipeline/transcoder.hxx>
#include <StormByte/multimedia/name_thread.hxx>
#include <StormByte/multimedia/exception.hxx>
#include <StormByte/multimedia/pipeline/config/audio.hxx>
#include <StormByte/multimedia/pipeline/config/subtitle.hxx>
#include <StormByte/multimedia/pipeline/config/video.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/filters.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/progress.hxx>
#include <StormByte/multimedia/pipeline/remuxer.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/safe/memory_order.hxx>

#include <chrono>
#include <format>
#include <string>
#include <string_view>
#include <utility>

using namespace StormByte::Multimedia::Backend::Pipeline;
using StormByte::Logger::Level;

TranscoderSlot::TranscoderSlot(const TranscoderSlot& other)
: In(other.In), Out(other.Out), Kind(other.Kind), Source(other.Source),
	Config(other.Config ? other.Config->Clone() : StormByte::Safe::Unique<StormByte::Multimedia::Pipeline::Config::Base>{}),
	Filters(other.Filters), Settled(other.Settled) {}

TranscoderSlot::TranscoderSlot(TranscoderSlot&& other) noexcept = default;

TranscoderSlot::~TranscoderSlot() noexcept = default;

TranscoderSlot& TranscoderSlot::operator=(const TranscoderSlot& other) {
	if (this == &other)
		return *this;
	TranscoderSlot replacement(other);
	*this = std::move(replacement);
	return *this;
}

TranscoderSlot& TranscoderSlot::operator=(TranscoderSlot&& other) noexcept = default;

namespace {
	template<typename Optional>
	StormByte::Safe::String ErrorText(const Optional& error, std::string_view fallback) noexcept {
		if (error)
			return StormByte::Safe::String{std::string_view{error.value()}};
		return StormByte::Safe::String{fallback};
	}

	void JobLog(const StormByte::Safe::Shared<StormByte::Logger::Log>& log,
		Level level, std::string_view text) noexcept {
		if (!log)
			return;
		*log << level << text << std::endl;
	}

	void RegisterStage(StormByte::Multimedia::Pipeline::JobTelemetry& job,
		const StormByte::Multimedia::Pipeline::Step& stage) noexcept {
		auto metrics = stage.Telemetry();
		auto name = metrics->Origin();
		job.RegisterStage(std::move(name), std::move(metrics));
	}

	struct TelemetrySummary {
		StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::JobTelemetry> Metrics;
		StormByte::Safe::Shared<StormByte::Logger::Log> Logger;

		~TelemetrySummary() noexcept {
			if (!Metrics)
				return;
			Metrics->SampleMemory();
			if (!Logger)
				return;
			const std::string report = static_cast<std::string>(*Metrics);
			std::size_t begin = 0;
			while (begin < report.size()) {
				const std::size_t end = report.find('\n', begin);
				const std::size_t length = end == std::string::npos ? report.size() - begin : end - begin;
				JobLog(Logger, Level::Info, std::string_view{report}.substr(begin, length));
				if (end == std::string::npos)
					break;
				begin = end + 1;
			}
		}
	};

	const StormByte::Multimedia::Codec* LeafCodec(
		const StormByte::Multimedia::Pipeline::Config::Base* config) noexcept {
		if (const auto* video = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Video*>(config))
			return video->Codec();
		if (const auto* audio = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Audio*>(config))
			return audio->Codec();
		if (const auto* subtitle = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Subtitle*>(config))
			return subtitle->Codec();
		return nullptr;
	}

	void ConfigureEncoder(StormByte::Multimedia::Pipeline::Encoder& encoder,
		const StormByte::Multimedia::Pipeline::Config::Base& config) noexcept {
		if (config.Implementation().Encoder)
			encoder.Implementation(config.Implementation().Encoder.value());
		if (const auto* video = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Video*>(&config)) {
			if (video->CRF())
				encoder.CRF(*video->CRF());
			if (video->BitRate())
				encoder.BitRate(*video->BitRate());
			if (video->Preset())
				encoder.Preset(*video->Preset());
			if (video->Tune())
				encoder.Tune(*video->Tune());
			if (!video->FineTune().empty())
				encoder.FineTune(video->FineTune());
		}
		else if (const auto* audio = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Audio*>(&config)) {
			if (audio->BitRate())
				encoder.BitRate(*audio->BitRate());
			if (audio->MaxBitRate())
				encoder.MaxBitRate(*audio->MaxBitRate());
			if (audio->Preset())
				encoder.Preset(audio->Preset().value());
		}
	}

	void StampMuxTags(StormByte::Multimedia::Pipeline::Muxer& mux, int out,
		const StormByte::Multimedia::Pipeline::Config::Base& config) noexcept {
		if (config.Language())
			mux.Language(out, config.Language().value());
		if (config.Title())
			mux.Title(out, config.Title().value());
	}

	bool Stopping(const Transcoder& coordinator) noexcept {
		return coordinator.Cancel.load(StormByte::Safe::MemoryOrder::Acquire);
	}
}

Transcoder::Transcoder() noexcept
: Metrics(StormByte::Safe::MakeShared<StormByte::Multimedia::Pipeline::JobTelemetry>()) {}

Transcoder::~Transcoder() noexcept {
	RequestCancel();
	Join();
}

void Transcoder::Start(StormByte::Multimedia::Pipeline::Transcoder& job) noexcept {
	const auto current = Status.load(StormByte::Safe::MemoryOrder::Acquire);
	if (current == StormByte::Multimedia::Pipeline::Status::Running
		|| current == StormByte::Multimedia::Pipeline::Status::Paused) {
		job.Fail("Run was already called");
		return;
	}
	Cancel.store(false, StormByte::Safe::MemoryOrder::Release);
	Paused.store(false, StormByte::Safe::MemoryOrder::Release);
	m_measureHook = false;
	m_analyticsHook = false;
	{
		StormByte::Safe::UniqueLock lock(Lock);
			Clock = StormByte::Safe::MakeShared<StormByte::Multimedia::Pipeline::Progress>();
		if (!job.m_duration || job.m_duration.value() <= 0)
			Clock->BeginDurationCalculation();
	}
	Status.store(StormByte::Multimedia::Pipeline::Status::Running, StormByte::Safe::MemoryOrder::Release);
	m_worker = StormByte::Safe::Thread([this, &job]() {
		Run(job);
	});
}

void Transcoder::RequestCancel() noexcept {
	Cancel.store(true, StormByte::Safe::MemoryOrder::Release);
	Paused.store(false, StormByte::Safe::MemoryOrder::Release);
	PauseCv.notify_all();
}

void Transcoder::Join() noexcept {
	if (m_worker.joinable())
		m_worker.join();
}

void Transcoder::WaitIfPaused() noexcept {
	StormByte::Safe::UniqueLock wait(PauseMutex);
	PauseCv.wait(wait, [this]() {
		return Cancel.load(StormByte::Safe::MemoryOrder::Acquire)
			|| !Paused.load(StormByte::Safe::MemoryOrder::Acquire);
	});
}

void Transcoder::TickHooks(StormByte::Multimedia::Pipeline::Transcoder& job,
	StormByte::Multimedia::Pipeline::Filters& graph) noexcept {
	Metrics->SampleMemory();
	graph.ClockAnalytics();
	if (!Clock)
		return;
	if (!m_measureHook && Clock->HasMeasure() && Clock->MeasureComplete()) {
		m_measureHook = true;
		job.OnMeasureDone();
	}
	if (!m_analyticsHook && Clock->HasAnalytics() && Clock->AnalyticsComplete()) {
		m_analyticsHook = true;
		job.OnAnalyticsDone();
	}
	job.OnProgress();
}

void Transcoder::Run(StormByte::Multimedia::Pipeline::Transcoder& job) noexcept {
	NameThread("MM-Transcoder");
	Metrics->SampleMemory();
	TelemetrySummary summary{Metrics, job.Logger()};
	const auto started = std::chrono::steady_clock::now();
	JobLog(job.Logger(), Level::Notice, "running");
	job.OnConfigure();
	if (Status.load(StormByte::Safe::MemoryOrder::Acquire) == StormByte::Multimedia::Pipeline::Status::Error) {
			job.OnError(ErrorText(job.Error(), "configure failed"));
		return;
	}

	if (Stopping(*this)) {
		Status.store(StormByte::Multimedia::Pipeline::Status::Aborted, StormByte::Safe::MemoryOrder::Release);
		JobLog(job.Logger(), Level::Notice, "aborted");
		job.OnAborted();
		return;
	}

	if (!job.m_reader || !job.m_writer) {
		job.Fail("reader or writer is not set");
		job.OnError(ErrorText(job.Error(), "reader or writer is not set"));
		return;
	}

	for (const auto& slot : Mapped) {
		if (!slot.Config) {
			job.Fail("track origin " + std::to_string(slot.In) + " has no config");
			job.OnError(ErrorText(job.Error(), "incomplete map"));
			return;
		}
	}

	if (!job.m_duration || job.m_duration.value() <= 0) {
		Clock->BeginDurationCalculation();
		job.OnProgress();
	}
	auto built = job.EmptyPlan(std::move(job.m_reader), std::move(job.m_writer), job.m_duration);
	if (!built) {
		job.Fail("EmptyPlan returned an empty owner");
		job.OnError(job.Error().value());
		return;
	}
	Clock->SetDurationCalculation(std::nullopt);
	job.OnProgress();
	if (Stopping(*this)) {
		Status.store(StormByte::Multimedia::Pipeline::Status::Aborted, StormByte::Safe::MemoryOrder::Release);
		job.OnAborted();
		return;
	}
	job.m_armed = true;
	job.m_consult.reset();
	try {
		for (const auto& slot : Mapped)
			built->add(StormByte::Multimedia::Pipeline::Track(slot.In, *slot.Config));
	}
	catch (const StormByte::Exception& error) {
		job.Fail(error.what());
		job.OnError(ErrorText(job.Error(), "config clone failed"));
		return;
	}
	catch (...) {
		job.Fail("config clone failed");
		job.OnError(ErrorText(job.Error(), "config clone failed"));
		return;
	}
	job.OnPlan(*built);

	const auto start = job.OnStart();
	if (start != StormByte::Multimedia::Pipeline::Status::Running) {
		if (start == StormByte::Multimedia::Pipeline::Status::Error) {
			if (!job.Failed())
				job.Fail("OnStart rejected the job");
			job.OnError(ErrorText(job.Error(), "OnStart rejected the job"));
		}
		else if (start == StormByte::Multimedia::Pipeline::Status::Aborted) {
			Status.store(StormByte::Multimedia::Pipeline::Status::Aborted, StormByte::Safe::MemoryOrder::Release);
			JobLog(job.Logger(), Level::Notice, "aborted");
			job.OnAborted();
		}
		else {
			Status.store(StormByte::Multimedia::Pipeline::Status::Stopped, StormByte::Safe::MemoryOrder::Release);
		}
		return;
	}

	const auto& tube = job.ApplicationLog();
	auto demux = StormByte::Safe::MakeShared<StormByte::Multimedia::Pipeline::Demuxer>(tube, Clock);
	auto mux = StormByte::Safe::MakeShared<StormByte::Multimedia::Pipeline::Muxer>(tube);
	RegisterStage(*Metrics, *demux);
	RegisterStage(*Metrics, *mux);
	std::move(built) >> *demux;
	job.m_plan = demux->Plan();
	*demux >> *mux;

	if (demux->Failed()) {
		job.Fail(ErrorText(demux->Error(), "demux open failed"));
		job.OnError(ErrorText(job.Error(), "demux"));
		return;
	}
	if (mux->Failed()) {
		job.Fail(ErrorText(mux->Error(), "mux open failed"));
		job.OnError(ErrorText(job.Error(), "mux"));
		return;
	}
	if (Stopping(*this)) {
		Status.store(StormByte::Multimedia::Pipeline::Status::Aborted, StormByte::Safe::MemoryOrder::Release);
		JobLog(job.Logger(), Level::Notice, "aborted");
		job.OnAborted();
		return;
	}

	StormByte::Multimedia::Pipeline::Filters graph;

	struct EncodeLane {
		int In = -1;
		StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Decoder> Decoder;
		StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Encoder> Encoder;
	};
	std::vector<EncodeLane> lanes;
	std::vector<StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Remuxer>> remuxes;

	int muxIndex = 0;
	for (auto& slot : Mapped) {
		if (slot.Kind == StormByte::Multimedia::Type::Attachment)
			continue;

		StampMuxTags(*mux, muxIndex, *slot.Config);
		const StormByte::Multimedia::Codec* codec = LeafCodec(slot.Config.get());
		if (!codec) {
					auto remux = StormByte::Safe::MakeShared<StormByte::Multimedia::Pipeline::Remuxer>(tube, slot.In);
			RegisterStage(*Metrics, *remux);
			*demux >> *remux;
			*remux >> *mux;
			if (mux->Failed() || remux->Failed()) {
					job.Fail(ErrorText(mux->Error(), ErrorText(remux->Error(), "remux reserve failed")));
					job.OnError(ErrorText(job.Error(), "mux"));
				return;
			}
			auto stretch = graph.Between(demux, remux);
			for (const auto& filter : slot.Filters)
				stretch.Add(filter);
			remuxes.push_back(std::move(remux));
		}
		else {
					auto decoder = StormByte::Safe::MakeShared<StormByte::Multimedia::Pipeline::Decoder>(tube, slot.In);
					auto encoder = StormByte::Safe::MakeShared<StormByte::Multimedia::Pipeline::Encoder>(tube, muxIndex, *codec);
			RegisterStage(*Metrics, *decoder);
			RegisterStage(*Metrics, *encoder);
			ConfigureEncoder(*encoder, *slot.Config);
			if (slot.Config->Implementation().Decoder)
				decoder->Implementation(slot.Config->Implementation().Decoder.value());
			*demux >> *decoder;
			*encoder >> *mux;
			if (decoder->Failed() || encoder->Failed() || mux->Failed()) {
					job.Fail(ErrorText(decoder->Error(), ErrorText(encoder->Error(),
						ErrorText(mux->Error(), "encode reserve failed"))));
					job.OnError(ErrorText(job.Error(), "encode"));
				return;
			}
			auto stretch = graph.Between(decoder, encoder);
			for (const auto& filter : slot.Filters)
				stretch.Add(filter);
			EncodeLane lane;
			lane.In = slot.In;
			lane.Decoder = std::move(decoder);
			lane.Encoder = std::move(encoder);
			lanes.push_back(std::move(lane));
		}
		++muxIndex;
	}

	if (!mux->Armed())
		mux->Fail(StormByte::Safe::String{"muxer is not armed; missing encoder or remuxer >> muxer"});
	for (const auto& filter : Analytics)
		graph.Add(filter);
	graph.Close();
	const auto stages = graph.StageTelemetries();
	for (auto stage : stages)
		Metrics->RegisterStage(std::move(stage.Name), std::move(stage.Metrics));
	TickHooks(job, graph);

	while (!Stopping(*this)) {
		WaitIfPaused();
		if (Stopping(*this))
			break;
		if (demux->Failed()) {
				job.Fail(ErrorText(demux->Error(), "demux failed"));
			break;
		}
		if (mux->Failed()) {
				job.Fail(ErrorText(mux->Error(), "mux failed"));
			break;
		}

		bool dead = false;
		for (auto& lane : lanes) {
			if (lane.Decoder && lane.Decoder->Failed()) {
					job.Fail(ErrorText(lane.Decoder->Error(), "decoder failed"));
				dead = true;
				break;
			}
			if (lane.Encoder && lane.Encoder->Failed()) {
					job.Fail(ErrorText(lane.Encoder->Error(), "encoder failed"));
				dead = true;
				break;
			}
			if (lane.Encoder && *lane.Encoder)
				job.MarkSettled(lane.In, *lane.Encoder);
		}
		if (dead)
			break;
		TickHooks(job, graph);
		if (mux->Closed())
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	if (Stopping(*this)
		&& Status.load(StormByte::Safe::MemoryOrder::Acquire) != StormByte::Multimedia::Pipeline::Status::Error) {
		Status.store(StormByte::Multimedia::Pipeline::Status::Aborted, StormByte::Safe::MemoryOrder::Release);
		JobLog(job.Logger(), Level::Notice, "aborted");
		job.OnAborted();
		return;
	}

	if (job.Failed()) {
		job.OnError(ErrorText(job.Error(), "transcode failed"));
		return;
	}

	while (!Stopping(*this) && !graph.Idle()) {
		TickHooks(job, graph);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	Reports = graph.Reports();
	TickHooks(job, graph);

	Status.store(StormByte::Multimedia::Pipeline::Status::Done, StormByte::Safe::MemoryOrder::Release);
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - started).count();
	JobLog(job.Logger(), Level::Info, std::format("done tracks={} {}ms", Mapped.size(), ms));
	job.OnDone();
}

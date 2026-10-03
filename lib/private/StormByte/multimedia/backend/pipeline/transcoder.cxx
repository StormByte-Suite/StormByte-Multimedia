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

#include <chrono>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

using namespace StormByte::Multimedia::Backend::Pipeline;
using StormByte::Logger::Level;

namespace {
	void JobLog(const std::shared_ptr<StormByte::Logger::Log>& log,
		Level level, std::string_view text) noexcept {
		if (!log)
			return;
		*log << level << text << std::endl;
	}

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
			encoder.Implementation(*config.Implementation().Encoder);
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
				encoder.Preset(*audio->Preset());
		}
	}

	void StampMuxTags(StormByte::Multimedia::Pipeline::Muxer& mux, int out,
		const StormByte::Multimedia::Pipeline::Config::Base& config) noexcept {
		if (config.Language())
			mux.Language(out, *config.Language());
		if (config.Title())
			mux.Title(out, *config.Title());
	}

	bool Stopping(const Transcoder& coordinator, const std::stop_token& token) noexcept {
		return token.stop_requested() || coordinator.Cancel.load(std::memory_order_acquire);
	}
}

Transcoder::Transcoder() noexcept = default;

Transcoder::~Transcoder() noexcept {
	RequestCancel();
	Join();
}

void Transcoder::Start(StormByte::Multimedia::Pipeline::Transcoder& job) noexcept {
	const auto current = Status.load(std::memory_order_acquire);
	if (current == StormByte::Multimedia::Pipeline::Status::Running
		|| current == StormByte::Multimedia::Pipeline::Status::Paused) {
		job.Fail("Run was already called");
		return;
	}
	Cancel.store(false, std::memory_order_release);
	Paused.store(false, std::memory_order_release);
	m_measureHook = false;
	m_analyticsHook = false;
	Clock.reset();
	Status.store(StormByte::Multimedia::Pipeline::Status::Running, std::memory_order_release);
	m_worker = std::jthread([this, &job](std::stop_token token) {
		Run(job, token);
	});
}

void Transcoder::RequestCancel() noexcept {
	Cancel.store(true, std::memory_order_release);
	Paused.store(false, std::memory_order_release);
	if (m_worker.joinable())
		m_worker.request_stop();
	PauseCv.notify_all();
}

void Transcoder::Join() noexcept {
	if (m_worker.joinable())
		m_worker.join();
}

void Transcoder::WaitIfPaused() noexcept {
	std::unique_lock wait(PauseMutex);
	PauseCv.wait(wait, [this]() {
		return Cancel.load(std::memory_order_acquire)
			|| !Paused.load(std::memory_order_acquire);
	});
}

void Transcoder::TickHooks(StormByte::Multimedia::Pipeline::Transcoder& job,
	StormByte::Multimedia::Pipeline::Filters& graph) noexcept {
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

void Transcoder::Run(StormByte::Multimedia::Pipeline::Transcoder& job, std::stop_token token) noexcept {
	NameThread("MM-Transcoder");
	const auto started = std::chrono::steady_clock::now();
	JobLog(job.Logger(), Level::Notice, "running");
	job.OnConfigure();
	if (Status.load(std::memory_order_acquire) == StormByte::Multimedia::Pipeline::Status::Error) {
		job.OnError(job.Error().value_or("configure failed"));
		return;
	}

	if (Stopping(*this, token)) {
		Status.store(StormByte::Multimedia::Pipeline::Status::Aborted, std::memory_order_release);
		JobLog(job.Logger(), Level::Notice, "aborted");
		job.OnAborted();
		return;
	}

	if (!job.m_reader || !job.m_writer) {
		job.Fail("reader or writer is not set");
		job.OnError(job.Error().value_or("reader or writer is not set"));
		return;
	}

	for (const auto& slot : Mapped) {
		if (!slot.Config) {
			job.Fail("track origin " + std::to_string(slot.In) + " has no config");
			job.OnError(job.Error().value_or("incomplete map"));
			return;
		}
	}

	auto built = job.EmptyPlan(std::move(job.m_reader), std::move(job.m_writer));
	job.m_armed = true;
	job.m_consult.reset();
	for (const auto& slot : Mapped)
		built->add(StormByte::Multimedia::Pipeline::Track(slot.In, *slot.Config));
	job.OnPlan(*built);

	const auto start = job.OnStart();
	if (start != StormByte::Multimedia::Pipeline::Status::Running) {
		if (start == StormByte::Multimedia::Pipeline::Status::Error) {
			if (!job.Failed())
				job.Fail("OnStart rejected the job");
			job.OnError(job.Error().value_or("OnStart rejected the job"));
		}
		else if (start == StormByte::Multimedia::Pipeline::Status::Aborted) {
			Status.store(StormByte::Multimedia::Pipeline::Status::Aborted, std::memory_order_release);
			JobLog(job.Logger(), Level::Notice, "aborted");
			job.OnAborted();
		}
		else {
			Status.store(StormByte::Multimedia::Pipeline::Status::Stopped, std::memory_order_release);
		}
		return;
	}

	const auto& tube = job.ApplicationLog();
	auto demux = std::make_shared<StormByte::Multimedia::Pipeline::Demuxer>(tube);
	auto mux = std::make_shared<StormByte::Multimedia::Pipeline::Muxer>(tube);
	std::move(*built) >> *demux;
	job.m_plan = demux->Plan();
	*demux >> *mux;
	Clock = std::const_pointer_cast<StormByte::Multimedia::Pipeline::Progress>(demux->Progress());

	if (demux->Failed()) {
		job.Fail(demux->Error().value_or("demux open failed"));
		job.OnError(job.Error().value_or("demux"));
		return;
	}
	if (mux->Failed()) {
		job.Fail(mux->Error().value_or("mux open failed"));
		job.OnError(job.Error().value_or("mux"));
		return;
	}
	if (Stopping(*this, token)) {
		Status.store(StormByte::Multimedia::Pipeline::Status::Aborted, std::memory_order_release);
		JobLog(job.Logger(), Level::Notice, "aborted");
		job.OnAborted();
		return;
	}

	StormByte::Multimedia::Pipeline::Filters graph;

	struct EncodeLane {
		int In = -1;
		std::shared_ptr<StormByte::Multimedia::Pipeline::Decoder> Decoder;
		std::shared_ptr<StormByte::Multimedia::Pipeline::Encoder> Encoder;
	};
	std::vector<EncodeLane> lanes;
	std::vector<std::shared_ptr<StormByte::Multimedia::Pipeline::Remuxer>> remuxes;

	int muxIndex = 0;
	for (auto& slot : Mapped) {
		if (slot.Kind == StormByte::Multimedia::Type::Attachment)
			continue;

		StampMuxTags(*mux, muxIndex, *slot.Config);
		const StormByte::Multimedia::Codec* codec = LeafCodec(slot.Config.get());
		if (!codec) {
			auto remux = std::make_shared<StormByte::Multimedia::Pipeline::Remuxer>(tube, slot.In);
			*demux >> *remux;
			*remux >> *mux;
			if (mux->Failed() || remux->Failed()) {
				job.Fail(mux->Error().value_or(remux->Error().value_or("remux reserve failed")));
				job.OnError(job.Error().value_or("mux"));
				return;
			}
			auto stretch = graph.Between(demux, remux);
			for (const auto& filter : slot.Filters)
				stretch.Add(filter);
			remuxes.push_back(std::move(remux));
		}
		else {
			auto decoder = std::make_shared<StormByte::Multimedia::Pipeline::Decoder>(tube, slot.In);
			auto encoder = std::make_shared<StormByte::Multimedia::Pipeline::Encoder>(tube, muxIndex, *codec);
			ConfigureEncoder(*encoder, *slot.Config);
			*demux >> *decoder;
			*encoder >> *mux;
			if (decoder->Failed() || encoder->Failed() || mux->Failed()) {
				job.Fail(decoder->Error().value_or(encoder->Error().value_or(
					mux->Error().value_or("encode reserve failed"))));
				job.OnError(job.Error().value_or("encode"));
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
		mux->Fail("muxer is not armed; missing encoder or remuxer >> muxer");
	for (const auto& filter : Analytics)
		graph.Add(filter);
	graph.Close();
	TickHooks(job, graph);

	while (!Stopping(*this, token)) {
		WaitIfPaused();
		if (Stopping(*this, token))
			break;
		if (demux->Failed()) {
			job.Fail(demux->Error().value_or("demux failed"));
			break;
		}
		if (mux->Failed()) {
			job.Fail(mux->Error().value_or("mux failed"));
			break;
		}

		bool dead = false;
		for (auto& lane : lanes) {
			if (lane.Decoder && lane.Decoder->Failed()) {
				job.Fail(lane.Decoder->Error().value_or("decoder failed"));
				dead = true;
				break;
			}
			if (lane.Encoder && lane.Encoder->Failed()) {
				job.Fail(lane.Encoder->Error().value_or("encoder failed"));
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

	if (Stopping(*this, token)
		&& Status.load(std::memory_order_acquire) != StormByte::Multimedia::Pipeline::Status::Error) {
		Status.store(StormByte::Multimedia::Pipeline::Status::Aborted, std::memory_order_release);
		JobLog(job.Logger(), Level::Notice, "aborted");
		job.OnAborted();
		return;
	}

	if (job.Failed()) {
		job.OnError(job.Error().value_or("transcode failed"));
		return;
	}

	while (!Stopping(*this, token) && !graph.Idle()) {
		TickHooks(job, graph);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	Reports = graph.Reports();
	TickHooks(job, graph);

	Status.store(StormByte::Multimedia::Pipeline::Status::Done, std::memory_order_release);
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - started).count();
	JobLog(job.Logger(), Level::Info, std::format("done tracks={} {}ms", Mapped.size(), ms));
	job.OnDone();
}

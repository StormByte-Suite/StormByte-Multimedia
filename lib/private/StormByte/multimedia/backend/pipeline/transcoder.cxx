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

#include <StormByte/multimedia/backend/pipeline/transcoder.hxx>
#include <StormByte/multimedia/backend/pipeline/ceiling.hxx>
#include <StormByte/multimedia/backend/pipeline/topology.hxx>
#include <StormByte/multimedia/backend/pipeline/topology_access.hxx>
#include <StormByte/multimedia/detail/locale.hxx>
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
#include <StormByte/safe/string.hxx>

#include <chrono>
#include <exception>
#include <new>
#include <string>
#include <string_view>
#include <utility>

using namespace StormByte::Multimedia::Backend::Pipeline;
using StormByte::Logger::Level;

TranscoderSlot::TranscoderSlot(const TranscoderSlot& other)
: In(other.In), Out(other.Out), Kind(other.Kind), Source(other.Source), Origin(other.Origin),
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
		try {
			if (error)
				return StormByte::Safe::String{std::string_view{*error}};
			return StormByte::Safe::String{fallback};
		} catch (const StormByte::Safe::AllocationError&) {
			return {};
		} catch (const std::bad_alloc&) {
			return {};
		}
	}

	void JobLog(const StormByte::Safe::Shared<StormByte::Logger::Log>& log,
		Level level, std::string_view text) {
		if (!log)
			return;
		*log << level << text << std::endl;
	}

	void RegisterStage(StormByte::Multimedia::Pipeline::JobTelemetry& job,
		const StormByte::Multimedia::Pipeline::Step& stage) {
		auto metrics = stage.Telemetry();
		auto name = metrics->Origin();
		job.RegisterStage(std::move(name), std::move(metrics));
	}

	using StormByte::Multimedia::Pipeline::TopologyFilterKind;
	using StormByte::Multimedia::Pipeline::TopologyStageKind;

	using NamedStages = StormByte::Safe::Vector<StormByte::Multimedia::Pipeline::TelemetryStage>;

	// Reuses the JobTelemetry::Stages display name for the same counters, else the origin.
	StormByte::Safe::String StageName(const NamedStages& named,
		const StormByte::Safe::Shared<const StormByte::Multimedia::Pipeline::StageTelemetry>& metrics) {
		for (const auto& entry : named)
			if (entry.Metrics.get() == metrics.get())
				return entry.Name;
		return metrics->Origin();
	}

	void DescribeStage(Topology& topology, const NamedStages& named, const StormByte::Multimedia::Pipeline::Step& stage,
		TopologyStageKind kind, StormByte::Safe::Optional<int> track = {},
		StormByte::Safe::Optional<int> output = {},
		StormByte::Safe::Optional<enum StormByte::Multimedia::Type> media = {}) {
		TopologyDescriptor descriptor;
		descriptor.Kind = kind;
		descriptor.Track = track;
		descriptor.OutputTrack = output;
		descriptor.Media = media;
		descriptor.Metrics = stage.Telemetry();
		descriptor.Name = StageName(named, descriptor.Metrics);
		descriptor.Probe = TopologyAccess::Probe(stage);
		topology.Register(std::move(descriptor));
	}

	void DescribeFilter(Topology& topology, const NamedStages& named,
		const StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Filter::FFmpeg>& filter,
		StormByte::Safe::Optional<int> track, StormByte::Safe::Optional<int> output,
		StormByte::Safe::Optional<std::uint32_t> position) {
		namespace Filter = StormByte::Multimedia::Pipeline::Filter;
		if (!filter)
			return;
		TopologyDescriptor descriptor;
		descriptor.Kind = TopologyStageKind::Filter;
		if (dynamic_cast<const Filter::Analytics*>(filter.get()))
			descriptor.Filter = TopologyFilterKind::Analytics;
		else if (dynamic_cast<const Filter::Packet*>(filter.get()))
			descriptor.Filter = TopologyFilterKind::Packet;
		else if (dynamic_cast<const Filter::Process*>(filter.get()))
			descriptor.Filter = TopologyFilterKind::Process;
		else
			descriptor.Filter = TopologyFilterKind::Other;
		descriptor.Track = track;
		descriptor.OutputTrack = output;
		descriptor.Media = filter->Media();
		descriptor.FilterPosition = position;
		descriptor.FilterName = filter->Leaf();
		descriptor.Metrics = filter->Telemetry();
		descriptor.Name = StageName(named, descriptor.Metrics);
		descriptor.Probe = TopologyAccess::Probe(*filter);
		topology.Register(std::move(descriptor));
	}

	struct TopologyFreeze {
		StormByte::Safe::Shared<Topology> Recorder;

		~TopologyFreeze() noexcept {
			if (Recorder)
				Recorder->Freeze();
		}
	};

	struct TelemetrySummary {
		StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::JobTelemetry> Metrics;
		StormByte::Safe::Shared<StormByte::Logger::Log> Logger;

		~TelemetrySummary() noexcept {
			if (std::uncaught_exceptions() != 0)
				return;
			try {
				if (!Metrics)
					return;
				Metrics->SampleMemory();
				if (!Logger)
					return;
				const auto ownedReport = static_cast<StormByte::Safe::String>(*Metrics);
				const std::string_view report{ownedReport};
				std::size_t begin = 0;
				while (begin < report.size()) {
					const std::size_t end = report.find('\n', begin);
					const std::size_t length = end == std::string::npos ? report.size() - begin : end - begin;
					JobLog(Logger, Level::Info, report.substr(begin, length));
					if (end == std::string::npos)
						break;
					begin = end + 1;
				}
			} catch (const StormByte::Safe::AllocationError&) {
			} catch (const std::bad_alloc&) {
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
		const StormByte::Multimedia::Pipeline::Config::Base& config) {
		if (config.Implementation().Encoder)
			encoder.Implementation(config.Implementation().Encoder.value());
		if (const auto* video = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Video*>(&config)) {
			if (video->CRF())
				encoder.CRF(*video->CRF());
			if (video->BitRate())
				encoder.BitRate(*video->BitRate());
			if (video->MaxBitRate())
				encoder.MaxBitRate(*video->MaxBitRate());
			if (video->Preset())
				encoder.Preset(*video->Preset());
			if (video->Tune())
				encoder.Tune(*video->Tune());
			if (!video->FineTune().empty())
				encoder.FineTune(video->FineTune());
		}
		else if (const auto* audio = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Audio*>(&config)) {
			if (audio->CRF())
				encoder.CRF(*audio->CRF());
			if (audio->BitRate())
				encoder.BitRate(*audio->BitRate());
			if (audio->MaxBitRate())
				encoder.MaxBitRate(*audio->MaxBitRate());
			if (audio->Preset())
				encoder.Preset(audio->Preset().value());
			if (!audio->FineTune().empty())
				encoder.FineTune(audio->FineTune());
		}
		else if (const auto* subtitle = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Subtitle*>(&config)) {
			if (!subtitle->FineTune().empty())
				encoder.FineTune(subtitle->FineTune());
		}
	}

	void StampMuxTags(StormByte::Multimedia::Pipeline::Muxer& mux, int out,
		const StormByte::Multimedia::Pipeline::Config::Base& config,
		const StormByte::Safe::Optional<StormByte::Multimedia::Metadata::Stream>& origin) {
		if (config.Language() && config.Language()->empty())
			mux.RemoveLanguage(out);
		else if (config.Language())
			mux.Language(out, config.Language().value());
		else if (origin && origin->Language())
			mux.Language(out, origin->Language().value());
		if (config.Title() && config.Title()->empty())
			mux.RemoveTitle(out);
		else if (config.Title())
			mux.Title(out, config.Title().value());
		else if (origin && origin->Title())
			mux.Title(out, origin->Title().value());
		if (!origin)
			return;
		using StormByte::Multimedia::Metadata::DispositionFlag;
		auto disposition = origin->Disposition();
		if (config.Default())
			*config.Default() ? disposition.Add(DispositionFlag::Default) : disposition.Remove(DispositionFlag::Default);
		if (config.Forced())
			*config.Forced() ? disposition.Add(DispositionFlag::Forced) : disposition.Remove(DispositionFlag::Forced);
		mux.Disposition(out, std::move(disposition));
	}

	bool Stopping(const Transcoder& coordinator) noexcept {
		return coordinator.Cancel.load(StormByte::Safe::MemoryOrder::Acquire);
	}
}

Transcoder::Transcoder()
: Metrics(StormByte::Safe::MakeShared<StormByte::Multimedia::Pipeline::JobTelemetry>()) {}

Transcoder::~Transcoder() noexcept {
	RequestCancel();
	Join();
}

void Transcoder::Start(StormByte::Multimedia::Pipeline::Transcoder& job) noexcept {
	try {
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
		if (!job.m_duration || *job.m_duration <= 0)
			Clock->BeginDurationCalculation();
	}
	Status.store(StormByte::Multimedia::Pipeline::Status::Running, StormByte::Safe::MemoryOrder::Release);
	m_worker = StormByte::Safe::Thread([this, &job]() {
		StormByte::Multimedia::Detail::UseCNumericLocale();
		Run(job);
	});
	} catch (const StormByte::Safe::AllocationError&) {
		job.Fail();
		job.OnError({});
	} catch (const std::bad_alloc&) {
		job.Fail();
		job.OnError({});
	}
}

void Transcoder::RequestCancel() noexcept {
	{
		StormByte::Safe::UniqueLock lock(PauseMutex);
		Cancel.store(true, StormByte::Safe::MemoryOrder::Release);
		Paused.store(false, StormByte::Safe::MemoryOrder::Release);
	}
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
	try {
	NameThread("MM-Transcoder");
	Metrics->SampleMemory();
	TelemetrySummary summary{Metrics, job.Logger()};
	// Declared before every stage owner: freezes after all stages are torn down.
	TopologyFreeze topologyFreeze;
	try {
		topologyFreeze.Recorder = Metrics->TopologyRecorder();
		topologyFreeze.Recorder->Reset();
	}
	catch (...) {
		topologyFreeze.Recorder.reset();
	}
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
	auto built = job.EmptyPlan(std::move(job.m_reader), std::move(job.m_writer), job.m_duration,
		job.m_container);
	if (!built) {
		job.Fail("EmptyPlan returned an empty owner");
		job.OnError(ErrorText(job.Error(), "EmptyPlan returned an empty owner"));
		return;
	}
	built->Resources(job.Resources());
	if (built->State() == StormByte::Multimedia::Pipeline::Plan::State::Created
		&& !built->Buffering(job.Buffering())) {
		job.Fail("buffering policy could not be applied to the new plan");
		return;
	}
	{
		StormByte::Safe::UniqueLock lock(Lock);
		job.m_plan = built;
		if (Paused.load(StormByte::Safe::MemoryOrder::Acquire))
			built->Hold(true);
		if (Status.load(StormByte::Safe::MemoryOrder::Acquire) == StormByte::Multimedia::Pipeline::Status::Error)
			built->State(StormByte::Multimedia::Pipeline::Plan::State::Failed, Error);
		else if (Stopping(*this))
			built->State(StormByte::Multimedia::Pipeline::Plan::State::Aborted);
	}
	auto failUnlessFinished = [built]() noexcept {
		if (built->State() != StormByte::Multimedia::Pipeline::Plan::State::Finished
			&& built->State() != StormByte::Multimedia::Pipeline::Plan::State::Aborted)
			built->State(StormByte::Multimedia::Pipeline::Plan::State::Failed, {});
	};
	struct PlanCompletion {
		decltype(failUnlessFinished)* action;

		~PlanCompletion() noexcept {
			(*action)();
		}
	} completion{&failUnlessFinished};
	StormByte::Multimedia::Pipeline::Filters graph;
	auto finishCancellation = [&]() {
		if (!Stopping(*this))
			return false;
		graph.Halt();
		built->Halt();
		if (!job.Failed() && built->State() == StormByte::Multimedia::Pipeline::Plan::State::Failed)
			job.Fail(ErrorText(built->Error(), "plan failed"));
		{
			StormByte::Safe::UniqueLock lock(Lock);
			if (Status.load(StormByte::Safe::MemoryOrder::Acquire) != StormByte::Multimedia::Pipeline::Status::Error)
				Status.store(StormByte::Multimedia::Pipeline::Status::Aborted, StormByte::Safe::MemoryOrder::Release);
		}
		if (job.Failed())
			job.OnError(ErrorText(job.Error(), "transcode failed"));
		else {
			JobLog(job.Logger(), Level::Notice, "aborted");
			job.OnAborted();
		}
		return true;
	};
	Clock->SetDurationCalculation(std::nullopt);
	job.OnProgress();
	if (finishCancellation())
		return;
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
	if (finishCancellation())
		return;

	const auto start = job.OnStart();
	if (start != StormByte::Multimedia::Pipeline::Status::Running) {
		if (start == StormByte::Multimedia::Pipeline::Status::Error) {
			if (!job.Failed())
				job.Fail("OnStart rejected the job");
			job.OnError(ErrorText(job.Error(), "OnStart rejected the job"));
		}
		else if (start == StormByte::Multimedia::Pipeline::Status::Aborted) {
			built->State(StormByte::Multimedia::Pipeline::Plan::State::Aborted);
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
	for (const auto& [tag, value] : FileTags) {
		if (value.empty())
			mux->RemoveTag(tag);
		else
			mux->Tag(tag, value);
	}
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
	if (finishCancellation())
		return;

	struct EncodeLane {
		int Out = -1;
		StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Decoder> Decoder;
		StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Encoder> Encoder;
	};
	std::vector<EncodeLane> lanes;
	std::vector<StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Remuxer>> remuxes;
	auto stopStages = [&]() noexcept {
		demux->Stop();
		mux->Stop();
		for (auto& lane : lanes) {
			lane.Decoder->Stop();
			lane.Encoder->Stop();
		}
		for (auto& remux : remuxes)
			remux->Stop();
		graph.Halt();
	};
	struct StageCancellation {
		decltype(stopStages)& Action;

		~StageCancellation() noexcept {
			Action();
		}
	} cancellation{stopStages};

	int muxIndex = 0;
	for (auto& slot : Mapped) {
		if (slot.Kind == StormByte::Multimedia::Type::Attachment)
			continue;

		StampMuxTags(*mux, muxIndex, *slot.Config, slot.Origin);
		const StormByte::Multimedia::Codec* codec = LeafCodec(slot.Config.get());
		if (!codec) {
			auto remux = StormByte::Safe::MakeShared<StormByte::Multimedia::Pipeline::Remuxer>(tube, slot.In, muxIndex);
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
			decoder->Implementation(slot.Config->Implementation().Decoder.value_or(StormByte::Safe::String{}));
			decoder->Acceleration(slot.Config->Implementation().Acceleration);
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
			lane.Out = slot.Out;
			lane.Decoder = std::move(decoder);
			lane.Encoder = std::move(encoder);
			lanes.push_back(std::move(lane));
		}
		++muxIndex;
	}

	for (const auto& filter : Analytics)
		graph.Add(filter);
	if (finishCancellation())
		return;
	graph.BindPlan(built);
	graph.Close();
	built >> *demux;
	if (finishCancellation())
		return;
	if (built->State(StormByte::Multimedia::Pipeline::Plan::State::Running)
		!= StormByte::Multimedia::Pipeline::Plan::State::Running) {
		job.Fail(ErrorText(built->Error(), "plan could not start"));
		job.OnError(ErrorText(job.Error(), "plan could not start"));
		return;
	}
	const auto stages = graph.StageTelemetries();
	for (auto stage : stages)
		Metrics->RegisterStage(std::move(stage.Name), std::move(stage.Metrics));
	// Topology is diagnostic only: an allocation failure leaves it partial, never fails the job.
	if (topologyFreeze.Recorder) try {
		auto& topology = *topologyFreeze.Recorder;
		const auto named = Metrics->Stages();
		DescribeStage(topology, named, *demux, TopologyStageKind::Demuxer);
		struct Stretch {
			int In;
			int Out;
			enum StormByte::Multimedia::Type Media;
		};
		std::vector<Stretch> stretches;
		std::size_t laneIndex = 0;
		std::size_t remuxIndex = 0;
		int outputIndex = 0;
		for (const auto& slot : Mapped) {
			if (slot.Kind == StormByte::Multimedia::Type::Attachment)
				continue;
			const bool remuxed = LeafCodec(slot.Config.get()) == nullptr;
			if (remuxed && remuxIndex < remuxes.size())
				DescribeStage(topology, named, *remuxes[remuxIndex++], TopologyStageKind::Remuxer, slot.In, outputIndex, slot.Kind);
			else if (!remuxed && laneIndex < lanes.size())
				DescribeStage(topology, named, *lanes[laneIndex].Decoder, TopologyStageKind::Decoder, slot.In, outputIndex, slot.Kind);
			std::uint32_t position = 0;
			for (const auto& filter : slot.Filters)
				DescribeFilter(topology, named, filter, slot.In, outputIndex, position++);
			if (!remuxed && laneIndex < lanes.size())
				DescribeStage(topology, named, *lanes[laneIndex++].Encoder, TopologyStageKind::Encoder, slot.In, outputIndex, slot.Kind);
			stretches.push_back(Stretch{slot.In, outputIndex, slot.Kind});
			++outputIndex;
		}
		for (const auto& filter : Analytics)
			DescribeFilter(topology, named, filter, {}, {}, {});
		for (const auto& look : TopologyAccess::Looks(graph)) {
			if (look.Stretch < stretches.size())
				DescribeStage(topology, named, *look.Decoder, TopologyStageKind::InspectionDecoder,
					look.Track, stretches[look.Stretch].Out, stretches[look.Stretch].Media);
			else
				DescribeStage(topology, named, *look.Decoder, TopologyStageKind::InspectionDecoder, look.Track);
		}
		DescribeStage(topology, named, *mux, TopologyStageKind::Muxer);
	}
	catch (...) {
	}
	TickHooks(job, graph);
	auto captureStageFailure = [&]() {
		if (job.Failed())
			return;
		if (built->State() == StormByte::Multimedia::Pipeline::Plan::State::Failed) {
			job.Fail(ErrorText(built->Error(), "plan failed"));
			return;
		}
		if (demux->Failed()) {
			job.Fail(ErrorText(demux->Error(), "demux failed"));
			return;
		}
		if (mux->Failed()) {
			job.Fail(ErrorText(mux->Error(), "mux failed"));
			return;
		}
		for (auto& lane : lanes) {
			if (lane.Decoder && lane.Decoder->Failed()) {
				job.Fail(ErrorText(lane.Decoder->Error(), "decoder failed"));
				return;
			}
			if (lane.Encoder && lane.Encoder->Failed()) {
				job.Fail(ErrorText(lane.Encoder->Error(), "encoder failed"));
				return;
			}
		}
		for (auto& remux : remuxes) {
			if (remux && remux->Failed()) {
				job.Fail(ErrorText(remux->Error(), "remux failed"));
				return;
			}
		}
		for (const auto& slot : Mapped) {
			for (const auto& filter : slot.Filters) {
				if (filter && filter->Failed()) {
					job.Fail(ErrorText(filter->Error(), "track filter failed"));
					return;
				}
			}
		}
		for (const auto& filter : Analytics) {
			if (filter && filter->Failed()) {
				job.Fail(ErrorText(filter->Error(), "analytics filter failed"));
				return;
			}
		}
	};
	auto stagesStopped = [&]() noexcept {
		using StormByte::Multimedia::Pipeline::State;
		if (demux->Status() != State::Stopped || mux->Status() != State::Stopped)
			return false;
		for (const auto& lane : lanes) {
			if (lane.Decoder->Status() != State::Stopped || lane.Encoder->Status() != State::Stopped)
				return false;
		}
		for (const auto& remux : remuxes) {
			if (remux->Status() != State::Stopped)
				return false;
		}
		return true;
	};

	while (!Stopping(*this)) {
		WaitIfPaused();
		if (Stopping(*this))
			break;
		captureStageFailure();
		if (job.Failed())
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
				job.MarkSettled(lane.Out, *lane.Encoder);
		}
		if (!dead) {
			for (auto& remux : remuxes) {
				if (remux && remux->Failed()) {
					job.Fail(ErrorText(remux->Error(), "remux failed"));
					dead = true;
					break;
				}
			}
		}
		if (dead)
			break;
		TickHooks(job, graph);
		if (mux->Closed())
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	if (finishCancellation())
		return;
	captureStageFailure();
	if (finishCancellation())
		return;

	if (job.Failed()) {
		job.OnError(ErrorText(job.Error(), "transcode failed"));
		return;
	}

	while (!Stopping(*this) && (!graph.Idle() || !stagesStopped()
		|| (Clock && Clock->HasAnalytics() && !Clock->AnalyticsComplete()))) {
		captureStageFailure();
		if (job.Failed())
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
		}
		if (!dead) {
			for (auto& remux : remuxes) {
				if (remux && remux->Failed()) {
					job.Fail(ErrorText(remux->Error(), "remux failed"));
					dead = true;
					break;
				}
			}
		}
		if (dead)
			break;
		TickHooks(job, graph);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	if (finishCancellation())
		return;
	captureStageFailure();
	if (finishCancellation())
		return;

	if (job.Failed()) {
		job.OnError(ErrorText(job.Error(), "transcode failed"));
		return;
	}

	for (auto& lane : lanes)
		job.MarkSettled(lane.Out, *lane.Encoder);
	Reports = graph.Reports();
	TickHooks(job, graph);
	if (finishCancellation())
		return;
	captureStageFailure();
	if (finishCancellation())
		return;
	if (job.Failed()) {
		job.OnError(ErrorText(job.Error(), "transcode failed"));
		return;
	}
	{
		StormByte::Safe::UniqueLock lock(Lock);
		if (Stopping(*this)) {
			lock.unlock();
			finishCancellation();
			return;
		}
		if (built->State(StormByte::Multimedia::Pipeline::Plan::State::Finished)
			!= StormByte::Multimedia::Pipeline::Plan::State::Finished) {
			lock.unlock();
			job.Fail(ErrorText(built->Error(), "plan could not finish"));
			job.OnError(ErrorText(job.Error(), "plan could not finish"));
			return;
		}
		Status.store(StormByte::Multimedia::Pipeline::Status::Done, StormByte::Safe::MemoryOrder::Release);
	}

	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - started).count();
	JobLog(job.Logger(), Level::Info, StormByte::Safe::String::ReplaceToken("done tracks={} {}ms", Mapped.size(), ms));
	job.OnDone();
	} catch (const StormByte::Safe::AllocationError&) {
		job.Fail();
		const StormByte::Safe::String empty;
		job.OnError(empty);
	} catch (const std::bad_alloc&) {
		job.Fail();
		const StormByte::Safe::String empty;
		job.OnError(empty);
	}
}

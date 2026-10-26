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

#include <StormByte/multimedia/features.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/property/dovi.hxx>
#include <StormByte/multimedia/property/video.hxx>
#include <StormByte/multimedia/stream.hxx>
#include <tables/encoder/table.hxx>

#include <StormByte/expected.hxx>
#include <StormByte/multimedia/backend/local_file_reader.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/cover.hxx>
#include <StormByte/multimedia/codec.hxx>
#include <StormByte/multimedia/pipeline/config/attachment.hxx>
#include <StormByte/multimedia/pipeline/config/audio.hxx>
#include <StormByte/multimedia/pipeline/config/base.hxx>
#include <StormByte/multimedia/pipeline/config/subtitle.hxx>
#include <StormByte/multimedia/pipeline/config/video.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/exception.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/registry.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/safe/condition_variable.hxx>
#include <StormByte/safe/unique_lock.hxx>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

extern "C" {
	#include <libavformat/avformat.h>
}

using StormByte::Buffer::IO::BufferedLocationReader;
using StormByte::Buffer::IO::BufferedLocationWriter;
using namespace StormByte::Multimedia::Pipeline;
using StormByte::Multimedia::Feature;
using StormByte::Multimedia::Features;

/**
 * @class Plan::Observers
 * @brief Borrowed stage registrations copied under the state mutex before notification.
 */
class Plan::Observers {
	public:
		std::vector<Step*> Stages;	///< Stages whose owners serialize teardown with notification.
		std::vector<Step*> Completed;
		StormByte::Safe::Mutex HoldMutex;					///< Guards HoldCv waits.
		StormByte::Safe::ConditionVariable HoldCv;			///< Wakes held stages.
		StormByte::Safe::Atomic<bool> Held{false};			///< Stages pause before their next work unit.
};

namespace {
	StormByte::Safe::Unique<BufferedLocationReader> LocalReader(const StormByte::Safe::String& path) {
		return StormByte::Multimedia::Backend::MakeLocalFileReader(path);
	}

	StormByte::Safe::Unique<BufferedLocationWriter> LocalWriter(const StormByte::Safe::String& path) {
		return StormByte::Multimedia::Backend::MakeLocalFileWriter(path);
	}

	std::string ExtensionOf(std::string_view location) noexcept {
		const auto end = location.find_first_of("?#");
		if (end != std::string_view::npos)
			location = location.substr(0, end);
		const auto separator = location.find_last_of("/\\");
		const auto dot = location.find_last_of('.');
		if (dot == std::string_view::npos || (separator != std::string_view::npos && dot < separator))
			return {};
		std::string ext(location.substr(dot + 1));
		for (char& ch : ext)
			ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		return ext;
	}

	const StormByte::Multimedia::Container* ContainerFromWriter(
		const BufferedLocationWriter& writer) noexcept {
		const auto ext = ExtensionOf(std::string_view{writer.Path()});
		if (ext.empty())
			return nullptr;
		auto found = StormByte::Multimedia::Registry::Instance().FindContainer(ext);
		if (!found)
			return nullptr;
		return &found.value().get();
	}

	const StormByte::Multimedia::Container* FormatIdentity(const AVOutputFormat& format) noexcept {
		const std::string_view all{format.name ? format.name : ""};
		const StormByte::Multimedia::Container* best = nullptr;
		int bestScore = -1;
		std::string_view names = all;
		while (!names.empty()) {
			const auto separator = names.find(',');
			const auto token = names.substr(0, separator);
			const auto found = StormByte::Multimedia::Registry::Instance().FindContainer(token);
			if (found) {
				const auto& container = found.value().get();
				int score = token == container.Extension() ? 10 : 0;
				std::string_view rest = all;
				while (!rest.empty()) {
					const auto next = rest.find(',');
					const auto piece = rest.substr(0, next);
					const auto same = StormByte::Multimedia::Registry::Instance().FindContainer(piece);
					if (same && same.value().get().Name() == container.Name())
						++score;
					if (next == std::string_view::npos)
						break;
					rest.remove_prefix(next + 1);
				}
				if (score > bestScore) {
					best = &container;
					bestScore = score;
				}
			}
			if (separator == std::string_view::npos)
				break;
			names.remove_prefix(separator + 1);
		}
		return best;
	}

	bool FormatServes(const AVOutputFormat& format,
		const StormByte::Multimedia::Container& container) noexcept {
		std::string_view names{format.name ? format.name : ""};
		while (!names.empty()) {
			const auto separator = names.find(',');
			const auto token = names.substr(0, separator);
			if (token == container.Extension())
				return true;
			const auto found = StormByte::Multimedia::Registry::Instance().FindContainer(token);
			if (found && found.value().get().Name() == container.Name())
				return true;
			if (separator == std::string_view::npos)
				break;
			names.remove_prefix(separator + 1);
		}
		return false;
	}

	const AVOutputFormat* IdentityFormat(const StormByte::Multimedia::Container& container) noexcept {
		const auto registered = StormByte::Multimedia::Registry::Instance().FindContainer(container.Name());
		if (!registered || registered.value().get().Name() != container.Name()
			|| !container.HasAccess(StormByte::Multimedia::Operation::Write))
			return nullptr;

		// extensions[0] / the first FFmpeg id is the muxer we mean to open.
		if (!container.Extension().empty()) {
			const std::string intended{container.Extension()};
			if (const auto* format = av_guess_format(intended.c_str(), nullptr, nullptr)) {
				if (!(format->flags & AVFMT_NOFILE) && FormatServes(*format, container))
					return format;
			}
		}

		const AVOutputFormat* fallback = nullptr;
		void* cursor = nullptr;
		while (const auto* format = av_muxer_iterate(&cursor)) {
			if (!format->name || (format->flags & AVFMT_NOFILE) || !FormatServes(*format, container))
				continue;
			if (std::string_view{format->name}.find(',') == std::string_view::npos)
				return format;
			if (!fallback)
				fallback = format;
		}
		return fallback;
	}

	const StormByte::Multimedia::Codec* DestinationCodec(
		const StormByte::Multimedia::Pipeline::Config::Base* config) noexcept {
		if (!config)
			return nullptr;
		if (const auto* video = dynamic_cast<const Config::Video*>(config))
			return video->Codec();
		if (const auto* audio = dynamic_cast<const Config::Audio*>(config))
			return audio->Codec();
		if (const auto* sub = dynamic_cast<const Config::Subtitle*>(config))
			return sub->Codec();
		return nullptr;
	}

	StormByte::Safe::Optional<StormByte::Safe::String> AttachmentMime(
		const StormByte::Multimedia::Pipeline::Config::Base* config) noexcept {
		if (!config)
			return std::nullopt;
		if (const auto* att = dynamic_cast<const Config::Attachment*>(config))
			return att->MimeType();
		return std::nullopt;
	}

	Features SourceFeatures(const StormByte::Multimedia::Stream& stream) noexcept {
		Features need;
		const auto video = stream.Video();
		if (!video)
			return need;
		if (video->HDR10()) {
			need.Add(Feature::HDR10);
			if (video->HDR10()->IsHDR10Plus())
				need.Add(Feature::HDR10Plus);
		}
		if (video->DOVI() && video->DOVI()->Present())
			need.Add(Feature::DOVI);
		return need;
	}

	bool CodecKeeps(std::string_view codec, std::string_view pin, const Features& need) noexcept {
		const auto keeps = [&](std::span<const StormByte::Multimedia::Tables::Encoder::EncoderDef> rows) noexcept {
			for (const auto& row : rows) {
				if (!row.codec || codec != row.codec)
					continue;
				if (!pin.empty() && pin != row.name)
					continue;
				if (row.features.Has(need))
					return true;
			}
			return false;
		};
		return keeps(StormByte::Multimedia::Tables::Encoder::Video())
			|| keeps(StormByte::Multimedia::Tables::Encoder::Audio())
			|| keeps(StormByte::Multimedia::Tables::Encoder::Subtitle());
	}

	std::string EncoderPin(const Config::Base* config) {
		if (!config || !config->Implementation().Encoder)
			return {};
		return std::string{std::string_view{*config->Implementation().Encoder}};
	}
}

Plan::Plan(const StormByte::Safe::String& source,
	const StormByte::Safe::String& destination,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
: Plan(LocalReader(source), LocalWriter(destination), std::move(duration)) {}

Plan::Plan(const StormByte::Safe::String& source,
	StormByte::Safe::Unique<BufferedLocationWriter> writer,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
: Plan(LocalReader(source), std::move(writer), std::move(duration)) {}

Plan::Plan(StormByte::Safe::Unique<BufferedLocationReader> reader,
	const StormByte::Safe::String& destination,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
: Plan(std::move(reader), LocalWriter(destination), std::move(duration)) {}

Plan::Plan(StormByte::Safe::Unique<BufferedLocationReader> reader,
	StormByte::Safe::Unique<BufferedLocationWriter> writer,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
: Plan(std::move(reader), std::move(writer), std::move(duration), nullptr) {}

Plan::Plan(StormByte::Safe::Unique<BufferedLocationReader> reader,
	StormByte::Safe::Unique<BufferedLocationWriter> writer,
	StormByte::Safe::Optional<std::int64_t> duration,
	const StormByte::Safe::Function<void(double)>* progress) noexcept
: Plan(std::move(reader), std::move(writer), static_cast<const class Container*>(nullptr),
	std::move(duration), progress) {}

Plan::Plan(const StormByte::Safe::String& source,
	const StormByte::Safe::String& destination, const class Container& container,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
: Plan(LocalReader(source), LocalWriter(destination), container, std::move(duration)) {}

Plan::Plan(const StormByte::Safe::String& source,
	StormByte::Safe::Unique<BufferedLocationWriter> writer, const class Container& container,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
: Plan(LocalReader(source), std::move(writer), container, std::move(duration)) {}

Plan::Plan(StormByte::Safe::Unique<BufferedLocationReader> reader,
	const StormByte::Safe::String& destination, const class Container& container,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
: Plan(std::move(reader), LocalWriter(destination), container, std::move(duration)) {}

Plan::Plan(StormByte::Safe::Unique<BufferedLocationReader> reader,
	StormByte::Safe::Unique<BufferedLocationWriter> writer, const class Container& container,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
: Plan(std::move(reader), std::move(writer), container, std::move(duration), nullptr) {}

Plan::Plan(StormByte::Safe::Unique<BufferedLocationReader> reader,
	StormByte::Safe::Unique<BufferedLocationWriter> writer, const class Container& container,
	StormByte::Safe::Optional<std::int64_t> duration,
	const StormByte::Safe::Function<void(double)>* progress) noexcept
: Plan(std::move(reader), std::move(writer), &container, std::move(duration), progress) {}

Plan::Plan(StormByte::Safe::Unique<BufferedLocationReader> reader,
	StormByte::Safe::Unique<BufferedLocationWriter> writer, const class Container* container,
	StormByte::Safe::Optional<std::int64_t> duration,
	const StormByte::Safe::Function<void(double)>* progress) noexcept
: m_input_telemetry(reader ? reader->Telemetry() : StormByte::Safe::Shared<StormByte::Buffer::ReadTelemetry>{}),
	m_output_telemetry(writer ? writer->Telemetry() : StormByte::Safe::Shared<StormByte::Buffer::WriteTelemetry>{}),
	m_reader(std::move(reader)),
	m_writer(std::move(writer)) {
	const AVOutputFormat* format = nullptr;
	if (container)
		format = IdentityFormat(*container);
	else if (m_writer)
		container = ContainerFromWriter(*m_writer);
	if (container && !format)
		format = IdentityFormat(*container);
	if (container && format && !(format->flags & AVFMT_NOFILE)) {
		m_container = container;
		// Keep the intended id, not FFmpeg's first alias in a comma-separated name.
		if (format->name && std::string_view{format->name}.find(',') == std::string_view::npos)
			m_format = format->name;
		else if (!container->Extension().empty())
			m_format = container->Extension().data();
		else
			m_format = format->name;
	}
	else if (container)
		m_container = container;
	if (!m_reader || !m_writer || !m_container || !m_format) {
		static_cast<void>(Validate(false));
		return;
	}
	if (duration && duration.value() <= 0)
		duration.reset();
	auto opened = StormByte::Multimedia::File::Open(*m_reader, duration);
	if (opened) {
		m_snapshot = StormByte::Safe::Unique<StormByte::Multimedia::File>::MakePointer<StormByte::Multimedia::File>(std::move(*opened));
		if (!duration) {
			if (progress)
				static_cast<void>(m_snapshot->Duration(*progress));
			else
				static_cast<void>(m_snapshot->Duration());
		}
	}
	static_cast<void>(Validate(false));
}

Plan::~Plan() noexcept = default;

enum Plan::State Plan::State() const noexcept {
	return m_state.load(StormByte::Safe::MemoryOrder::Acquire);
}

enum Plan::State Plan::State(enum State desired,
	StormByte::Safe::Optional<StormByte::Safe::String> reason) const noexcept {
	if (desired == State::Running && !Check())
		return State();
	StormByte::Safe::UniqueLock lock(m_state_mutex);
	const auto current = m_state.load(StormByte::Safe::MemoryOrder::Acquire);
	const bool accepted = (current == State::Created && (desired == State::Running || desired == State::Failed))
		|| (current == State::Running && (desired == State::Finished || desired == State::Failed))
		|| (current == State::Failed && desired == State::Failed);
	if (accepted) {
		if (desired == State::Failed && reason && !m_error)
			m_error = std::move(reason);
		m_state.store(desired, StormByte::Safe::MemoryOrder::Release);
	}
	if (accepted && m_observers)
		for (auto* observer : m_observers->Stages)
			observer->Wake();
	return accepted ? desired : current;
}

StormByte::Safe::Optional<StormByte::Safe::String> Plan::Error() const noexcept {
	StormByte::Safe::UniqueLock lock(m_state_mutex);
	return m_error;
}

void Plan::Observe(Step& step) noexcept {
	StormByte::Safe::UniqueLock lock(m_state_mutex);
	if (!m_observers)
		m_observers = StormByte::Safe::Unique<Observers>::MakePointer<Observers>();
	auto& stages = m_observers->Stages;
	if (std::find(stages.begin(), stages.end(), &step) == stages.end())
		stages.push_back(&step);
}

void Plan::Hold(bool held) noexcept {
	Observers* observers = nullptr;
	{
		StormByte::Safe::UniqueLock lock(m_state_mutex);
		if (!m_observers)
			m_observers = StormByte::Safe::Unique<Observers>::MakePointer<Observers>();
		observers = m_observers.get();
	}
	{
		StormByte::Safe::UniqueLock lock(observers->HoldMutex);
		observers->Held.store(held, StormByte::Safe::MemoryOrder::Release);
	}
	observers->HoldCv.notify_all();
}

void Plan::WaitWhileHeld(const Step& step) const noexcept {
	Observers* observers = nullptr;
	{
		StormByte::Safe::UniqueLock lock(m_state_mutex);
		observers = m_observers.get();
	}
	if (!observers)
		return;
	StormByte::Safe::UniqueLock lock(observers->HoldMutex);
	// Timed so cancellation and failure, which do not signal HoldCv, still release the stage.
	while (observers->Held.load(StormByte::Safe::MemoryOrder::Acquire) && !step.Stopping()
		&& State() == State::Running)
		observers->HoldCv.wait_for(lock, std::chrono::milliseconds(100));
}

void Plan::Unobserve(Step& step) noexcept {
	StormByte::Safe::UniqueLock lock(m_state_mutex);
	if (!m_observers)
		return;
	auto& stages = m_observers->Stages;
	const auto found = std::find(stages.begin(), stages.end(), &step);
	if (found != stages.end())
		stages.erase(found);
}

void Plan::Complete(Step& step, bool success) noexcept {
	if (!success) {
		State(State::Failed, step.Error().value_or(StormByte::Safe::String{"pipeline stage stopped before completion"}));
		return;
	}
	StormByte::Safe::UniqueLock lock(m_state_mutex);
	if (!m_observers || State() != State::Running)
		return;
	auto& completed = m_observers->Completed;
	if (std::find(completed.begin(), completed.end(), &step) == completed.end())
		completed.push_back(&step);
	if (completed.size() != m_observers->Stages.size())
		return;
	m_state.store(State::Finished, StormByte::Safe::MemoryOrder::Release);
	for (auto* observer : m_observers->Stages)
		observer->Wake();
}

BufferedLocationReader& Plan::Reader() noexcept {
	return *m_reader;
}

const BufferedLocationReader& Plan::Reader() const noexcept {
	return *m_reader;
}

StormByte::Safe::Shared<StormByte::Buffer::ReadTelemetry> Plan::InputTelemetry() const noexcept {
	return m_input_telemetry;
}

StormByte::Safe::Shared<StormByte::Buffer::WriteTelemetry> Plan::OutputTelemetry() const noexcept {
	return m_output_telemetry;
}

BufferedLocationWriter& Plan::Writer() noexcept {
	return *m_writer;
}

const BufferedLocationWriter& Plan::Writer() const noexcept {
	return *m_writer;
}

const StormByte::Multimedia::File& Plan::Snapshot() const noexcept {
	return *m_snapshot;
}

Plan::operator bool() const noexcept {
	return Check().has_value();
}

CheckResult Plan::Check() const {
	return Validate(true);
}

CheckResult Plan::Validate(bool tracks) const {
	const auto fail = [this](const char* reason) -> CheckResult {
		State(State::Failed, StormByte::Safe::String{reason});
		const auto error = Error().value_or(StormByte::Safe::String{reason});
		return StormByte::Unexpected<PlanException>("{}", std::string_view{error});
	};
	if (State() == State::Failed)
		return fail("plan has failed");
	if (!m_reader || !m_writer)
		return fail("plan location owner is empty");
	if (!m_container)
		return fail("destination container is unknown");
	if (!m_format)
		return fail("destination container has no registered buffered FFmpeg muxer");
	if (!m_snapshot)
		return fail("source snapshot failed");
	if (!tracks)
		return CheckResult{};
	if (m_tracks.empty())
		return fail("plan has no tracks");

	for (const auto& held : m_tracks) {
		if (!held)
			return fail("plan holds an empty track");
		const Track& track = *held;
		if (track.In() < 0)
			return fail("track origin index is negative");
		if (track.Type() == StormByte::Multimedia::Type::Unknown)
			return fail("track type is unknown");

		if (track.Type() == StormByte::Multimedia::Type::Attachment) {
			const auto mime = AttachmentMime(track.Config());
			if (!mime || mime->empty())
				return fail("attachment MIME is empty");
			if (!Detail::MimePatternOk(*mime))
				return fail("attachment MIME is not exact, type-star or star-star");
			continue;
		}

		const auto* dest = DestinationCodec(track.Config());
		if (dest && dest->Type() != track.Type())
			return fail("destination codec type does not match the track");
		if (!dest || !m_policies.Has(Policy::EnforceFeatures))
			continue;
		const auto& streams = m_snapshot->Streams();
		for (std::size_t index = 0; index < streams.size(); ++index) {
			if (streams[index].Index() != track.In())
				continue;
			const Features need = SourceFeatures(streams[index]);
			if (!need.Has(Feature::HDR10) && !need.Has(Feature::HDR10Plus) && !need.Has(Feature::DOVI))
				break;
			if (!CodecKeeps(dest->Name(), EncoderPin(track.Config()), need))
				return fail("destination codec drops source features");
			break;
		}
	}

	return CheckResult{};
}

Demuxer& StormByte::Multimedia::Pipeline::operator>>(StormByte::Safe::Shared<Plan> plan, Demuxer& demuxer) noexcept {
	if (!plan) {
		demuxer.Fail(StormByte::Safe::String{"plan owner is empty"});
		return demuxer;
	}

	demuxer.Plan(std::move(plan));
	return demuxer;
}

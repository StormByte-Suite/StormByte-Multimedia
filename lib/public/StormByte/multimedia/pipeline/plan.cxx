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

#include <StormByte/multimedia/pipeline/plan.hxx>

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
#include <StormByte/multimedia/registry.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/safe/wstring.hxx>

#include <cctype>
#include <optional>
#include <string>
#include <utility>

using StormByte::Buffer::IO::BufferedLocationReader;
using StormByte::Buffer::IO::BufferedLocationWriter;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	StormByte::Safe::String LocationText(const std::filesystem::path& path) {
		const auto native = path.wstring();
		return StormByte::Safe::String{StormByte::Safe::WString{std::wstring_view{native}}};
	}

	StormByte::Safe::Unique<BufferedLocationReader> LocalReader(const std::filesystem::path& path) {
		return StormByte::Multimedia::Backend::MakeLocalFileReader(LocationText(path));
	}

	StormByte::Safe::Unique<BufferedLocationWriter> LocalWriter(const std::filesystem::path& path) {
		return StormByte::Multimedia::Backend::MakeLocalFileWriter(LocationText(path));
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
}

Plan::Plan(const std::filesystem::path& source,
	const std::filesystem::path& destination,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
: Plan(LocalReader(source), LocalWriter(destination), std::move(duration)) {}

Plan::Plan(const std::filesystem::path& source,
	StormByte::Safe::Unique<BufferedLocationWriter> writer,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept
: Plan(LocalReader(source), std::move(writer), std::move(duration)) {}

Plan::Plan(StormByte::Safe::Unique<BufferedLocationReader> reader,
	const std::filesystem::path& destination,
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
: m_input_telemetry(reader ? reader->Telemetry() : StormByte::Safe::Shared<StormByte::Buffer::ReadTelemetry>{}),
	m_output_telemetry(writer ? writer->Telemetry() : StormByte::Safe::Shared<StormByte::Buffer::WriteTelemetry>{}),
	m_reader(std::move(reader)),
	m_writer(std::move(writer)),
	m_container(m_writer ? ContainerFromWriter(*m_writer) : nullptr) {
	if (!m_reader)
		return;
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
}

Plan::Plan(Plan&& other) noexcept = default;

Plan::~Plan() noexcept = default;

StormByte::Safe::Shared<Plan> Plan::Move() {
	return StormByte::Safe::Shared<Plan>::MakePointer<Plan>(std::move(*this));
}

const StormByte::Multimedia::Container* Plan::ContainerFromWriter(
	const BufferedLocationWriter& writer) noexcept {
	const auto ext = ExtensionOf(std::string_view{writer.Path()});
	if (ext.empty())
		return nullptr;
	auto found = StormByte::Multimedia::Registry::Instance().FindContainer(ext);
	if (!found)
		return nullptr;
	return &found.value().get();
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
	if (!m_reader || !m_writer)
		return StormByte::Unexpected<PlanException>("plan was moved-from");
	if (!m_snapshot)
		return StormByte::Unexpected<PlanException>("source snapshot failed");
	if (m_writer->Path().empty())
		return StormByte::Unexpected<PlanException>("destination path is empty");
	if (!m_container)
		return StormByte::Unexpected<PlanException>("destination container is unknown");
	if (m_tracks.empty())
		return StormByte::Unexpected<PlanException>("plan has no tracks");

	for (const auto& held : m_tracks) {
		if (!held)
			return StormByte::Unexpected<PlanException>("plan holds an empty track");
		const Track& track = *held;
		if (track.In() < 0)
			return StormByte::Unexpected<PlanException>("track origin index is negative");
		if (track.Type() == StormByte::Multimedia::Type::Unknown)
			return StormByte::Unexpected<PlanException>("track type is unknown");

		if (track.Type() == StormByte::Multimedia::Type::Attachment) {
			const auto mime = AttachmentMime(track.Config());
			if (!mime || mime->empty())
				return StormByte::Unexpected<PlanException>("attachment MIME is empty");
			if (!Detail::MimePatternOk(*mime))
				return StormByte::Unexpected<PlanException>("attachment MIME is not exact, type-star or star-star");
			continue;
		}

		const auto* dest = DestinationCodec(track.Config());
		if (dest && dest->Type() != track.Type())
			return StormByte::Unexpected<PlanException>("destination codec type does not match the track");
	}

	return CheckResult{};
}

Demuxer& StormByte::Multimedia::Pipeline::operator>>(Plan&& plan, Demuxer& demuxer) noexcept {
	return plan.Move() >> demuxer;
}

Demuxer& StormByte::Multimedia::Pipeline::operator>>(StormByte::Safe::Shared<Plan> plan, Demuxer& demuxer) noexcept {
	if (demuxer.Plan()) {
		demuxer.Fail(StormByte::Safe::String{"demuxer already has a plan"});
		return demuxer;
	}
	if (!plan) {
		demuxer.Fail(StormByte::Safe::String{"plan owner is empty"});
		return demuxer;
	}

	Step& step = demuxer;
	step.m_plan = std::move(plan);
	demuxer.m_planPresent.notify_all();
	demuxer.Wake();
	return demuxer;
}

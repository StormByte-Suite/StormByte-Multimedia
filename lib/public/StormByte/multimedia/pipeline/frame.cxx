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

#include <StormByte/multimedia/backend/pipeline/frame.hxx>
#include <StormByte/multimedia/backend/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>

#include <utility>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

Frame::Frame()
: Item(-1, Type::Unknown, Kind::Frame, Producer::Decoder), m_part(0) {}

Frame::Frame(int track, enum Type type, enum Producer producer, StormByte::Buffer::FIFO payload,
	StormByte::Safe::Optional<Property::Duration> pts,
	StormByte::Safe::Optional<Property::Duration> duration,
	StormByte::Safe::Optional<Property::Video> video,
	StormByte::Safe::Vector<class SideData> attachments,
	StormByte::Safe::Optional<Property::Audio> audio,
	std::uint64_t serial,
	std::uint64_t part)
: Item(track, type, Kind::Frame, producer),
	m_payload(std::move(payload)),
	m_pts(std::move(pts)), m_duration(std::move(duration)),
	m_video(std::move(video)), m_audio(std::move(audio)),
	m_attachments(std::move(attachments)),
	m_serial(serial), m_part(part) {}

Frame::Frame(const Frame& other)
: Item(other),
	m_payload(other.m_payload),
	m_pts(other.m_pts),
	m_dts(other.m_dts),
	m_duration(other.m_duration),
	m_video(other.m_video),
	m_audio(other.m_audio),
	m_language(other.m_language),
	m_title(other.m_title),
	m_attachments(other.m_attachments),
	m_serial(other.m_serial),
	m_part(other.m_part) {
	if (other.m_backend)
		m_backend = std::make_unique<Backend::Pipeline::Frame>(*other.m_backend);
}

Frame::Frame(Frame&& other) noexcept
: Item(std::move(other)),
	m_payload(std::move(other.m_payload)),
	m_pts(std::move(other.m_pts)),
	m_dts(std::move(other.m_dts)),
	m_duration(std::move(other.m_duration)),
	m_video(std::move(other.m_video)),
	m_audio(std::move(other.m_audio)),
	m_language(std::move(other.m_language)),
	m_title(std::move(other.m_title)),
	m_attachments(std::move(other.m_attachments)),
	m_serial(std::move(other.m_serial)),
	m_part(other.m_part),
	m_backend(std::move(other.m_backend)) {
	other.BecomeEmpty();
}

Frame::~Frame() noexcept = default;

Frame& Frame::operator=(const Frame& other) {
	if (this == &other)
		return *this;
	Frame replacement(other);
	return *this = std::move(replacement);
}

Frame& Frame::operator=(Frame&& other) noexcept {
	if (this == &other)
		return *this;
	Item::operator=(std::move(other));
	m_payload = std::move(other.m_payload);
	m_pts = std::move(other.m_pts);
	m_dts = std::move(other.m_dts);
	m_duration = std::move(other.m_duration);
	m_video = std::move(other.m_video);
	m_audio = std::move(other.m_audio);
	m_language = std::move(other.m_language);
	m_title = std::move(other.m_title);
	m_attachments = std::move(other.m_attachments);
	m_serial = std::move(other.m_serial);
	m_part = other.m_part;
	m_backend = std::move(other.m_backend);
	other.BecomeEmpty();
	return *this;
}

void Frame::BecomeEmpty() noexcept {
	m_track = -1;
	m_type = Type::Unknown;
	m_kind = Kind::Frame;
	m_producer = Producer::Decoder;
	m_payload = StormByte::Buffer::FIFO{};
	m_part = 0;
	m_backend.reset();
}

StormByte::Buffer::FIFO& Frame::Payload() noexcept {
	if (m_backend && !m_backend->PayloadReady()) {
		StormByte::BinaryData bytes;
		m_backend->Handle().CopyPrimaryBuffer(bytes);
		m_payload = StormByte::Buffer::FIFO{std::move(bytes)};
		m_backend->PayloadReady(true);
	}

	return m_payload;
}

void Frame::Bind(std::unique_ptr<Backend::Pipeline::Frame> backend) noexcept {
	m_backend = std::move(backend);
}

Item::PointerType Frame::Clone() const {
	return Item::PointerType::MakePointer<Frame>(*this);
}

Item::PointerType Frame::Move() {
	return Item::PointerType::MakePointer<Frame>(std::move(*this));
}

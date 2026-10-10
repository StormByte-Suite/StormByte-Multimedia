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

#include <StormByte/multimedia/backend/pipeline/content.hxx>
#include <StormByte/multimedia/backend/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/property/audio.hxx>
#include <StormByte/multimedia/property/channel_layout.hxx>
#include <StormByte/multimedia/property/video.hxx>
#include <StormByte/multimedia/type.hxx>

#include <cstdint>

extern "C" {
	#include <libavutil/channel_layout.h>
	#include <libavutil/frame.h>
}

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Backend::Pipeline;

namespace {
	Property::ChannelLayout LayoutFromHandle(
		const StormByte::Multimedia::FFmpeg::AVFrame& handle,
		Property::ChannelLayout previous) noexcept {
		const std::uint64_t mask = handle.ChannelLayout().Mask();
		if (mask == AV_CH_LAYOUT_MONO)
			return Property::ChannelLayout::Mono;
		if (mask == AV_CH_LAYOUT_STEREO || mask == AV_CH_LAYOUT_STEREO_DOWNMIX)
			return Property::ChannelLayout::Stereo;
		if (mask == AV_CH_LAYOUT_2POINT1)
			return Property::ChannelLayout::TwoPointOne;
		if (mask == AV_CH_LAYOUT_SURROUND)
			return Property::ChannelLayout::ThreePointZero;
		if (mask == AV_CH_LAYOUT_4POINT0)
			return Property::ChannelLayout::FourPointZero;
		if (mask == AV_CH_LAYOUT_QUAD || mask == AV_CH_LAYOUT_2_2)
			return Property::ChannelLayout::Quad;
		if (mask == AV_CH_LAYOUT_5POINT0 || mask == AV_CH_LAYOUT_5POINT0_BACK)
			return Property::ChannelLayout::FivePointZero;
		if (mask == AV_CH_LAYOUT_5POINT1 || mask == AV_CH_LAYOUT_5POINT1_BACK)
			return Property::ChannelLayout::FivePointOne;
		if (mask == AV_CH_LAYOUT_6POINT1 || mask == AV_CH_LAYOUT_6POINT1_BACK
			|| mask == AV_CH_LAYOUT_6POINT1_FRONT)
			return Property::ChannelLayout::SixPointOne;
		if (mask == AV_CH_LAYOUT_7POINT1)
			return Property::ChannelLayout::SevenPointOne;
		if (mask == AV_CH_LAYOUT_7POINT1_WIDE || mask == AV_CH_LAYOUT_7POINT1_WIDE_BACK)
			return Property::ChannelLayout::SevenPointOneW;
		if (mask == AV_CH_LAYOUT_OCTAGONAL)
			return Property::ChannelLayout::Octagonal;
		if (mask == AV_CH_LAYOUT_22POINT2)
			return Property::ChannelLayout::TwentyTwoPointTwo;

		switch (handle.Channels()) {
			case 1: return Property::ChannelLayout::Mono;
			case 2: return Property::ChannelLayout::Stereo;
			case 6: return Property::ChannelLayout::FivePointOne;
			case 8: return Property::ChannelLayout::SevenPointOne;
			default: break;
		}

		if (Property::ChannelCount(previous) == static_cast<unsigned>(handle.Channels()))
			return previous;
		return Property::ChannelLayout::Unknown;
	}
}

Frame::Frame(const Frame& other) noexcept
: m_handle(other.m_handle), m_payloadReady(other.m_payloadReady), m_contentValid(other.m_contentValid) {}

Frame& Frame::operator=(const Frame& other) noexcept {
	if (this == &other)
		return *this;
	m_handle = other.m_handle;
	m_payloadReady = other.m_payloadReady;
	m_contentValid = other.m_contentValid;
	return *this;
}

void Frame::BindProperties(StormByte::Multimedia::Pipeline::Frame& frame) noexcept try {
	if (!m_handle)
		return;

	switch (frame.Type()) {
		case Type::Video: {
			if (!frame.m_video)
				return;
			if (m_handle.Width() <= 0 || m_handle.Height() <= 0)
				return;
			const auto sar = m_handle.SampleAspectRatio();
			auto dovi = frame.m_video->DOVI();
			const auto* metadata = m_handle.SideData(AV_FRAME_DATA_DOVI_METADATA);
			const auto* rpu = m_handle.SideData(AV_FRAME_DATA_DOVI_RPU_BUFFER);
			if (metadata || rpu) {
				auto merged = dovi.value_or(Property::DOVI{});
				bool loaded = false;
				if (metadata && metadata->data)
					loaded = merged.LoadMetadata(std::as_bytes(std::span{metadata->data, metadata->size}));
				if (rpu && rpu->data)
					loaded = merged.LoadRpu(std::as_bytes(std::span{rpu->data, rpu->size})) || loaded;
				if (loaded)
					dovi = std::move(merged);
			}
			frame.m_video = Property::Video(
				frame.m_video->Color(),
				Property::Resolution{
					static_cast<std::uint32_t>(m_handle.Width()),
					static_cast<std::uint32_t>(m_handle.Height())
				},
				frame.m_video->HDR10(),
				frame.m_video->FrameRate(),
				sar.Valid() ? std::optional<Property::AVRational>{sar} : frame.m_video->SampleAspectRatio(),
				std::move(dovi));
			break;
		}

		case Type::Audio: {
			if (!frame.m_audio)
				return;
			if (m_handle.SampleRate() <= 0 || m_handle.Channels() <= 0)
				return;
			frame.m_audio = Property::Audio(
				LayoutFromHandle(m_handle, frame.m_audio->Layout()),
				static_cast<std::uint32_t>(m_handle.SampleRate()),
				static_cast<std::uint8_t>(m_handle.Channels()),
				frame.m_audio->BitRate(),
				frame.m_audio->Profile());
			break;
		}

		default:
			break;
	}
}

catch (...) {
	m_contentValid = false;
	m_warning = "failed to bind preserved frame metadata";
}

void Frame::Put(StormByte::Multimedia::Pipeline::Frame& owner, ::AVFrame* raw) noexcept {
	m_warning.clear();
	m_contentValid = true;
	const ::AVFrame* before = m_handle.Get();
	auto content = Content::For(owner.Type());
	content->Put(before, raw);
	m_warning = content->Warning();
	if (before && raw) {
		for (const auto type : {AV_FRAME_DATA_DOVI_METADATA, AV_FRAME_DATA_DOVI_RPU_BUFFER,
			AV_FRAME_DATA_MASTERING_DISPLAY_METADATA, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL}) {
			if (av_frame_get_side_data(before, type) && !av_frame_get_side_data(raw, type))
				m_contentValid = false;
		}
	}
	m_handle.Reset(raw);
	m_payloadReady = false;
	BindProperties(owner);
}

const std::string& Frame::Warning() const noexcept {
	return m_warning;
}

bool Frame::ContentValid() const noexcept {
	return m_contentValid;
}

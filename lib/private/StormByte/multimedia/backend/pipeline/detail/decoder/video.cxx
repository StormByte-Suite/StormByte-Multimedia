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

#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/decoder/video.hxx>
#include <StormByte/multimedia/backend/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/backend/ffmpeg/AVPool.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/side_data.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/property/hdr10.hxx>
#include <StormByte/multimedia/property/point.hxx>
#include <StormByte/multimedia/property/video.hxx>
#include <StormByte/multimedia/type.hxx>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

extern "C" {
	#include <libavutil/avutil.h>
	#include <libavutil/frame.h>
	#include <libavutil/mastering_display_metadata.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/rational.h>
}

using StormByte::Multimedia::Pipeline::DecoderFlag;
using StormByte::Multimedia::Pipeline::Producer;
using StormByte::Multimedia::Pipeline::SideData;
using StormByte::Multimedia::Pipeline::SideDataKind;
using StormByte::Multimedia::Type;
namespace FFmpeg = StormByte::Multimedia::FFmpeg;

namespace {
	constexpr int ChromaDenominator = 50000;
	constexpr int LuminanceDenominator = 10000;

	std::optional<StormByte::Multimedia::Property::Duration> TicksToPts(std::int64_t ticks, FFmpeg::AVRational timeBase) noexcept {
		if (ticks == AV_NOPTS_VALUE || ticks < 0 || timeBase.num <= 0 || timeBase.den <= 0)
			return std::nullopt;
		const std::int64_t ns = timeBase.Rescale(ticks, FFmpeg::AVRational{1, 1000000000});
		if (ns < 0)
			return std::nullopt;
		return StormByte::Multimedia::Property::Duration{std::chrono::nanoseconds{ns}};
	}

	std::optional<StormByte::Multimedia::Property::Duration> TicksToDuration(std::int64_t ticks, FFmpeg::AVRational timeBase) noexcept {
		if (ticks == AV_NOPTS_VALUE || ticks <= 0 || timeBase.num <= 0 || timeBase.den <= 0)
			return std::nullopt;
		const std::int64_t ns = timeBase.Rescale(ticks, FFmpeg::AVRational{1, 1000000000});
		if (ns <= 0)
			return std::nullopt;
		return StormByte::Multimedia::Property::Duration{std::chrono::nanoseconds{ns}};
	}

	std::int64_t NsToTicks(const StormByte::Safe::Optional<StormByte::Multimedia::Property::Duration>& value, FFmpeg::AVRational timeBase) noexcept {
		if (!value.has_value() || timeBase.num <= 0 || timeBase.den <= 0)
			return AV_NOPTS_VALUE;
		return FFmpeg::AVRational{1, 1000000000}.Rescale(value->Nanoseconds().count(), timeBase);
	}

	StormByte::Multimedia::Property::Point FromChromaPair(const ::AVRational& x, const ::AVRational& y) noexcept {
		return StormByte::Multimedia::Property::Point::Normalized(x.num, x.den, y.num, y.den, ChromaDenominator);
	}

	StormByte::Multimedia::Property::Point FromLuminancePair(const ::AVRational& minNits, const ::AVRational& maxNits) noexcept {
		return StormByte::Multimedia::Property::Point::Normalized(
			minNits.num, minNits.den, maxNits.num, maxNits.den, LuminanceDenominator);
	}

	SideDataKind MapKind(AVFrameSideDataType type) noexcept {
		switch (type) {
			case AV_FRAME_DATA_MASTERING_DISPLAY_METADATA:	return SideDataKind::MasteringDisplay;
			case AV_FRAME_DATA_CONTENT_LIGHT_LEVEL:			return SideDataKind::ContentLight;
			case AV_FRAME_DATA_DYNAMIC_HDR_PLUS:			return SideDataKind::HdrPlus;
			case AV_FRAME_DATA_DYNAMIC_HDR_VIVID:			return SideDataKind::HdrVivid;
			case AV_FRAME_DATA_A53_CC:						return SideDataKind::A53CC;
			case AV_FRAME_DATA_STEREO3D:					return SideDataKind::Stereo3D;
			case AV_FRAME_DATA_DISPLAYMATRIX:				return SideDataKind::DisplayMatrix;
			case AV_FRAME_DATA_ICC_PROFILE:					return SideDataKind::IccProfile;
			case AV_FRAME_DATA_S12M_TIMECODE:				return SideDataKind::S12MTimecode;
			case AV_FRAME_DATA_SPHERICAL:					return SideDataKind::Spherical;
			case AV_FRAME_DATA_SEI_UNREGISTERED:			return SideDataKind::SeiUnregistered;
			case AV_FRAME_DATA_FILM_GRAIN_PARAMS:			return SideDataKind::FilmGrain;
			case AV_FRAME_DATA_DOVI_RPU_BUFFER:				return SideDataKind::DolbyVisionRpu;
			case AV_FRAME_DATA_DOVI_METADATA:				return SideDataKind::DolbyVision;
			case AV_FRAME_DATA_AMBIENT_VIEWING_ENVIRONMENT:	return SideDataKind::AmbientViewing;
			default:										return SideDataKind::Other;
		}
	}

	StormByte::Safe::Vector<SideData> MapAttachments(const StormByte::Multimedia::FFmpeg::AVFrame& av) noexcept {
		StormByte::Safe::Vector<SideData> out;
		for (int i = 0; i < av.SideDataCount(); ++i) {
			const AVFrameSideData* sd = av.SideDataAt(i);
			if (!sd || !sd->data || sd->size <= 0)
				continue;
			StormByte::Safe::Binary bytes;
			bytes.assign(
				reinterpret_cast<const std::byte*>(sd->data),
				reinterpret_cast<const std::byte*>(sd->data) + sd->size);
			const auto kind = MapKind(sd->type);
			if (kind == SideDataKind::Other) {
				const char* name = av_frame_side_data_name(sd->type);
				out.emplace_back(StormByte::Safe::String(name ? name : "unknown"),
					StormByte::Buffer::FIFO{std::move(bytes)});
			}

			else
				out.emplace_back(kind, StormByte::Buffer::FIFO{std::move(bytes)});
		}

		return out;
	}

	std::optional<StormByte::Multimedia::Property::HDR10> MapFrameHDR10(
		const StormByte::Multimedia::FFmpeg::AVFrame& av, bool heuristics, const StormByte::Multimedia::Property::Video& video) noexcept {
		const AVFrameSideData* mdmSd = av.SideData(AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
		const AVFrameSideData* cllSd = av.SideData(AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
		const AVFrameSideData* plusSd = av.SideData(AV_FRAME_DATA_DYNAMIC_HDR_PLUS);
		const auto* mdm = (mdmSd && mdmSd->size >= sizeof(AVMasteringDisplayMetadata))
			? reinterpret_cast<const AVMasteringDisplayMetadata*>(mdmSd->data) : nullptr;
		const auto* cll = (cllSd && cllSd->size >= sizeof(AVContentLightMetadata))
			? reinterpret_cast<const AVContentLightMetadata*>(cllSd->data) : nullptr;
		const bool plus = plusSd != nullptr;

		std::optional<StormByte::Multimedia::Property::Point> light;
		if (cll && (cll->MaxCLL || cll->MaxFALL))
			light = StormByte::Multimedia::Property::Point{static_cast<int>(cll->MaxCLL), static_cast<int>(cll->MaxFALL)};

		if (mdm && mdm->has_primaries && mdm->has_luminance) {
			StormByte::Multimedia::Property::HDR10 out{
				FromChromaPair(mdm->display_primaries[0][0], mdm->display_primaries[0][1]),
				FromChromaPair(mdm->display_primaries[1][0], mdm->display_primaries[1][1]),
				FromChromaPair(mdm->display_primaries[2][0], mdm->display_primaries[2][1]),
				FromChromaPair(mdm->white_point[0], mdm->white_point[1]),
				FromLuminancePair(mdm->min_luminance, mdm->max_luminance),
				light ? light : (video.HDR10() ? video.HDR10()->LightLevel() : std::nullopt),
				StormByte::Multimedia::Property::HDR10::Source::Metadata
			};
			out.HDR10Plus(plus || (video.HDR10() && video.HDR10()->IsHDR10Plus()));
			return out;
		}

		if (video.HDR10()) {
			auto kept = *video.HDR10();
			if (plus)
				kept.HDR10Plus(true);
			if (light) {
				kept = StormByte::Multimedia::Property::HDR10{
					kept.Red(), kept.Green(), kept.Blue(), kept.White(), kept.Luminance(),
					light, kept.Origin()
				};
				if (plus || video.HDR10()->IsHDR10Plus())
					kept.HDR10Plus(true);
			}

			return kept;
		}

		if (heuristics && video.Color().IsHDR10()) {
			StormByte::Multimedia::Property::HDR10 out = StormByte::Multimedia::Property::HDR10::DEFAULT;
			if (light)
				out = StormByte::Multimedia::Property::HDR10{
					out.Red(), out.Green(), out.Blue(), out.White(), out.Luminance(),
					light, StormByte::Multimedia::Property::HDR10::Source::Heuristics
				};
			out.HDR10Plus(plus);
			return out;
		}

		return std::nullopt;
	}
}

namespace StormByte::Multimedia::Backend::Pipeline::Detail::Decoder {
	namespace FFmpeg = StormByte::Multimedia::FFmpeg;

	Video::~Video() noexcept {
		using StormByte::Multimedia::Backend::FFmpeg::AVPool;
		AVPool::ShutdownTrace("Video properties reset begin", this);
		m_video.reset();
		AVPool::ShutdownTrace("Video properties reset end; scratch member destructor next", this);
	}

	Video::Video(Video&& other) noexcept = default;

	Video& Video::operator=(Video&& other) noexcept = default;

	Video::Video(StormByte::Multimedia::FFmpeg::AVDecoder decoder, FFmpeg::AVRational timeBase,
		StormByte::Safe::Optional<StormByte::Multimedia::Property::Video> video) noexcept
	: m_decoder(std::move(decoder)), m_video(std::move(video)), m_timeBase(timeBase), m_draining(false), m_flushed(false) {}

	bool Video::IsOpen() const noexcept {
		return true;
	}

	bool Video::Send(StormByte::Multimedia::Pipeline::Decoder& owner,
		const StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet>& packet) noexcept {
		if (!packet) {
			owner.Fail("empty packet");
			return false;
		}

		StormByte::Multimedia::FFmpeg::AVPacket raw;
		StormByte::Safe::Binary bytes;
		const auto n = packet->Payload().Available();
		const std::uint8_t* data = nullptr;
		if (n > 0) {
			if (!packet->Payload().Extract(n, bytes) || bytes.size() != n) {
				owner.Fail("failed to extract packet payload");
				return false;
			}

			data = reinterpret_cast<const std::uint8_t*>(bytes.data());
		}

		if (!raw.Load(data, static_cast<int>(n), owner.Index(), packet->KeyFrame())) {
			owner.Fail("out of memory copying packet");
			return false;
		}

		const std::int64_t duration = packet->Duration()
			? FFmpeg::AVRational{1, 1000000000}.Rescale(packet->Duration()->Nanoseconds().count(), m_timeBase)
			: 0;
		raw.Timestamps(NsToTicks(packet->Pts(), m_timeBase), NsToTicks(packet->Dts(), m_timeBase), duration);

		const auto result = m_decoder.SendPacket(raw);
		if (result == StormByte::Multimedia::FFmpeg::OperationResult::Error) {
			owner.Fail("failed to send packet");
			return false;
		}

		if (result == StormByte::Multimedia::FFmpeg::OperationResult::TryAgain)
			return false;
		return true;
	}

	StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Frame> Video::Receive(
		StormByte::Multimedia::Pipeline::Decoder& owner) noexcept {
		auto holder = StormByte::Safe::MakeUnique<StormByte::Multimedia::Backend::Pipeline::Frame>();
		auto result = m_decoder.ReceiveFrame(holder->Handle());
		if (result == StormByte::Multimedia::FFmpeg::OperationResult::TryAgain && m_draining && !m_flushed) {
			Flush(owner);
			if (owner.Failed())
				return {};
			if (!m_flushed) {
				owner.Fail("decoder EOF submission made no progress after draining output");
				return {};
			}
			result = m_decoder.ReceiveFrame(holder->Handle());
		}
		if (result == StormByte::Multimedia::FFmpeg::OperationResult::TryAgain
			|| result == StormByte::Multimedia::FFmpeg::OperationResult::EndOfFile)
			return {};
		if (result != StormByte::Multimedia::FFmpeg::OperationResult::Success) {
			owner.Fail("failed to receive frame");
			return {};
		}

		auto video = m_video;
		auto attachments = MapAttachments(holder->Handle());
		if (video) {
			auto hdr = MapFrameHDR10(holder->Handle(),
				owner.Flags().Has(DecoderFlag::HeuristicsHDR10), *video);
			video = StormByte::Multimedia::Property::Video(
				video->Color(), video->Resolution(), std::move(hdr), video->FrameRate(),
				video->SampleAspectRatio(), video->DOVI());
		}

		auto frame = StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Frame>::MakePointer<
			StormByte::Multimedia::Pipeline::Frame>(
				owner.Index(),
				Type::Video,
				Producer::Decoder,
				StormByte::Buffer::FIFO{},
				TicksToPts(holder->Handle().Pts(), m_timeBase),
				TicksToDuration(holder->Handle().DurationTicks(), m_timeBase),
				std::move(video),
				std::move(attachments),
				std::nullopt,
				0,
				0);
		auto* backend = holder.get();
		BindFrame(owner, *frame, std::move(holder));
		backend->BindProperties(*frame);
		return frame;
	}

	void Video::Flush(StormByte::Multimedia::Pipeline::Decoder& owner) noexcept {
		if (owner.Failed() || m_flushed)
			return;
		m_draining = true;
		const auto sent = m_decoder.SetEof();
		if (sent == StormByte::Multimedia::FFmpeg::OperationResult::Success
			|| sent == StormByte::Multimedia::FFmpeg::OperationResult::EndOfFile) {
			m_flushed = true;
		}
		else if (sent == StormByte::Multimedia::FFmpeg::OperationResult::Error)
			owner.Fail("failed to flush decoder");
	}

	bool Video::Reset(StormByte::Multimedia::Pipeline::Decoder& owner) noexcept {
		if (owner.Failed())
			return false;
		m_decoder.Flush();
		m_scratch.Unref();
		m_draining = false;
		m_flushed = false;
		return true;
	}
}

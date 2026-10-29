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

#include <StormByte/multimedia/backend/pipeline/decoder.hxx>
#include <StormByte/multimedia/backend/pipeline/frame.hxx>
#include <StormByte/multimedia/ffmpeg/typedefs.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>

extern "C" {
	#include <libavutil/error.h>
}

using StormByte::Multimedia::Backend::Pipeline::Decoder;
using StormByte::Multimedia::Backend::Pipeline::Frame;
using PublicDecoder = StormByte::Multimedia::Pipeline::Decoder;
using PublicFrame = StormByte::Multimedia::Pipeline::Frame;

bool Decoder::RejectedPacket(PublicDecoder& owner,
	const StormByte::Multimedia::Pipeline::Packet& packet, int error) noexcept {
	const auto reason = error != 0 ? StormByte::Multimedia::FFmpeg::ErrorToString(error)
		: StormByte::Safe::String{"bitstream filter failed"};
	const auto message = StormByte::Safe::String::ReplaceToken(
		"packet rejected t={} serial={} bytes={}: {}",
		packet.Track(), packet.Serial().value_or(0),
		static_cast<std::size_t>(packet.Payload().Available()), std::string_view{reason});
	if (error == AVERROR_INVALIDDATA) {
		owner.Log(StormByte::Logger::Level::Warning,
			StormByte::Safe::String::ReplaceToken("{}; skipping packet and continuing decoding", std::string_view{message}));
		return true;
	}
	owner.Fail(message);
	return false;
}

void Decoder::BindFrame(PublicDecoder& owner, PublicFrame& frame,
	StormByte::Safe::Unique<Frame> holder) noexcept {
	owner.Attach(frame, std::move(holder));
}

void Decoder::CloseCue(PublicDecoder& owner, PublicFrame& frame,
	StormByte::Multimedia::Property::Duration duration) noexcept {
	owner.CloseCue(frame, std::move(duration));
}

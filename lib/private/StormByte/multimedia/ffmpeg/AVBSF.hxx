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

#pragma once

#include <StormByte/multimedia/ffmpeg/AVPointer.hxx>
#include <StormByte/multimedia/ffmpeg/AVRational.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/ffmpeg/backend_typedefs.hxx>

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	class AVCodecParameters;
	class AVPacket;

	/**
	 * @class AVBSF
	 * @brief RAII bitstream filter context.
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE AVBSF: public AVPointer<::AVBSFContext> {
		public:
			/**
			 * @brief Copy constructor (deleted).
			 */
			AVBSF(const AVBSF&) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Source filter.
			 */
			AVBSF(AVBSF&& other) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			~AVBSF() noexcept override;

			/**
			 * @brief Copy assignment (deleted).
			 * @return *this.
			 */
			AVBSF& operator=(const AVBSF&) = delete;

			/**
			 * @brief Move assignment.
			 * @param other Source filter.
			 * @return *this.
			 */
			AVBSF& operator=(AVBSF&& other) noexcept;

			/**
			 * @brief Creates and initializes a named BSF.
			 * @param name Filter name (e.g. "h264_mp4toannexb").
			 * @param params Input codec parameters.
			 * @param time_base Input time base.
			 * @return AVBSF or BSFError.
			 */
			static ExpectedAVBSF Create(std::string_view name, const AVCodecParameters& params, AVRational time_base) noexcept;

			/**
			 * @brief Sends a packet into the filter.
			 * @param pkt Packet to send.
			 * @return Operation result.
			 */
			OperationResult SendPacket(AVPacket& pkt) noexcept;

			/**
			 * @brief Receives a filtered packet.
			 * @param pkt Destination packet.
			 * @return Operation result.
			 */
			OperationResult ReceivePacket(AVPacket& pkt) noexcept;

			/**
			 * @brief Flushes the filter.
			 */
			void Flush() noexcept;

			/**
			 * @brief Signals EOF (null packet).
			 */
			void SetEof() noexcept;

		private:
			/**
			 * @brief Adopts an allocated BSF context.
			 * @param ctx Allocated BSF context.
			 */
			explicit AVBSF(AVBSFContext* ctx) noexcept;

			/**
			 * @brief Frees the BSF (av_bsf_free).
			 */
			void Free() noexcept override;

			using AVPointer<::AVBSFContext>::Get;
	};

	extern template class STORMBYTE_MULTIMEDIA_PRIVATE AVPointer<::AVBSFContext>;
}

/**
 * @brief Conditional provider contract: FFmpeg resources are released out-of-line.
 * @note Multimedia, Base and FFmpeg must remain loaded with compatible ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVBSF);

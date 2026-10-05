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

#pragma once

#include <StormByte/multimedia/ffmpeg/AVBSF.hxx>
#include <StormByte/multimedia/ffmpeg/typedefs.hxx>

#include <deque>

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	class AVPacket;

	/**
	 * @class AVBSFPipeline
	 * @brief Ordered chain of bitstream filters applied to packets.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC AVBSFPipeline {
		public:
			/**
			 * @brief Default constructor.
			 */
			AVBSFPipeline() noexcept;

			/**
			 * @brief Copy constructor (deleted).
			 */
			AVBSFPipeline(const AVBSFPipeline&) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Source pipeline.
			 */
			AVBSFPipeline(AVBSFPipeline&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			~AVBSFPipeline() noexcept;

			/**
			 * @brief Copy assignment (deleted).
			 * @return *this.
			 */
			AVBSFPipeline& operator=(const AVBSFPipeline&) = delete;

			/**
			 * @brief Move assignment.
			 * @param other Source pipeline.
			 * @return *this.
			 */
			AVBSFPipeline& operator=(AVBSFPipeline&& other) noexcept;

			/**
			 * @brief Appends a filter to the chain.
			 * @param bsf Filter to take ownership of.
			 */
			void Add(AVBSF&& bsf) noexcept;

			/**
			 * @brief Runs @p pkt through all filters in order.
			 * @param pkt Packet in/out.
			 * @return Operation result.
			 */
			OperationResult Process(AVPacket& pkt) noexcept;

			/**
			 * @brief Flushes every filter.
			 */
			void Flush() noexcept;

			/**
			 * @brief Signals EOF on every filter.
			 */
			void SetEof() noexcept;

			/**
			 * @brief Removes all filters.
			 */
			void Clear() noexcept;

			/**
			 * @brief Whether the chain is empty.
			 * @return true if no filters are registered.
			 */
			bool Empty() const noexcept;

		private:
			std::deque<AVBSF> m_filters;	///< Filter chain
	};
}

/**
 * @brief Conditional provider contract: private deque construction, movement and release stay in Multimedia.
 * @note Multimedia, Base and FFmpeg must remain loaded with compatible ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVBSFPipeline);

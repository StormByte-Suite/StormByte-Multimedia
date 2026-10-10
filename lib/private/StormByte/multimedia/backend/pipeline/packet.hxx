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

#include <StormByte/multimedia/ffmpeg/AVCodecParameters.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>

namespace StormByte::Multimedia::Pipeline {
	class Packet;
}

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @class Packet
	 * @brief Holder of one compressed access unit for the public Packet facade.
	 *
	 * May also own a deep copy of the producer @c AVCodecParameters
	 * (extradata included). Analytics encode-look uses that copy
	 * to open a decoder without a format context.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Packet {
		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Empty holder.
			 */
			Packet() noexcept = default;

			/**
			 * @brief Deep copy (cloned FFmpeg packet and parameters).
			 * @param other Source holder.
			 */
			Packet(const Packet& other) noexcept;

			/**
			 * @brief Move constructor.
			 * @param other Holder to take.
			 */
			Packet(Packet&& other) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			~Packet() noexcept = default;

			/**
			 * @brief Deep copy assignment (cloned FFmpeg packet and parameters).
			 * @param other Source holder.
			 * @return *this.
			 */
			Packet& operator=(const Packet& other) noexcept;

			/**
			 * @brief Move assignment.
			 * @param other Holder to take.
			 * @return *this.
			 */
			Packet& operator=(Packet&& other) noexcept = default;

			/**
			 * @}
			 */

			/**
			 * @name Handle
			 * @{
			 */

			/**
			 * @brief FFmpeg packet owned by this holder.
			 * @return Handle.
			 */
			inline const StormByte::Multimedia::FFmpeg::AVPacket& Handle() const noexcept {
				return m_handle;
			}

			/**
			 * @brief FFmpeg packet owned by this holder.
			 * @return Handle.
			 */
			inline StormByte::Multimedia::FFmpeg::AVPacket& Handle() noexcept {
				return m_handle;
			}

			/**
			 * @brief Replaces the owned FFmpeg packet.
			 * @param handle Packet to take.
			 */
			inline void Handle(StormByte::Multimedia::FFmpeg::AVPacket handle) noexcept {
				m_handle = std::move(handle);
			}

			/**
			 * @}
			 */

			/**
			 * @name Parameters
			 * @{
			 */

			/**
			 * @brief Codec parameters stamped by the producer, if any.
			 * @return Pointer valid while this holder lives, or nullptr.
			 */
			inline const StormByte::Multimedia::FFmpeg::AVCodecParameters* Parameters() const noexcept {
				if (!m_params)
					return nullptr;
				return &*m_params;
			}

			/**
			 * @brief Codec parameters stamped by the producer, if any.
			 * @return Pointer valid while this holder lives, or nullptr.
			 */
			inline StormByte::Multimedia::FFmpeg::AVCodecParameters* Parameters() noexcept {
				if (!m_params)
					return nullptr;
				return &*m_params;
			}

			/**
			 * @brief Deep-copies @p params onto this holder.
			 * @param params Source parameters. Empty clears the stamp.
			 */
			void Parameters(StormByte::Safe::Optional<StormByte::Multimedia::FFmpeg::AVCodecParameters> params) noexcept;

			/**
			 * @}
			 */

			/**
			 * @brief Hook after Replace. No high-level bag on a Packet.
			 * @param packet Public unit that owns this holder.
			 */
			void BindProperties(StormByte::Multimedia::Pipeline::Packet& packet) noexcept;

			/**
			 * @brief Copies original FFmpeg packet metadata without replacing the facade payload.
			 * @param packet Public unit that owns the original packet holder.
			 * @param destination FFmpeg packet receiving properties and side data.
			 * @return True when copied or no original holder exists, false on allocation failure.
			 */
			static bool CopyProperties(const StormByte::Multimedia::Pipeline::Packet& packet,
				::AVPacket* destination) noexcept;

		private:
			StormByte::Multimedia::FFmpeg::AVPacket m_handle;	///< FFmpeg packet
			StormByte::Safe::Optional<StormByte::Multimedia::FFmpeg::AVCodecParameters> m_params;	///< Producer codecpar.
	};
}

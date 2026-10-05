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

#include <StormByte/logger/log.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/pointers.hxx>

#include <cstddef>
#include <memory>

namespace StormByte::Multimedia::Backend::Pipeline::Detail::Worker {
	class Remux;
}

/**
 * @namespace StormByte::Multimedia::Pipeline
 * @brief Demux / decode / filter / encode / mux types.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Pipeline {
	class Demuxer;
	class Muxer;
	class Remuxer;

	/**
	 * @brief Binds origin track remuxer.In() from @p demuxer onto @p remuxer.
	 * @param demuxer Origin demuxer.
	 * @param remuxer Destination remuxer.
	 * @return @p remuxer.
	 */
	STORMBYTE_MULTIMEDIA_PUBLIC Remuxer& operator>>(Demuxer& demuxer, Remuxer& remuxer) noexcept;

	/**
	 * @class Remuxer
	 * @brief Forwards compressed packets of one origin track to the muxer.
	 *
	 * Packets retain their source lineage and are forwarded without
	 * re-encoding. @ref Filters can analyze decoded destination
	 * pictures without changing the forwarded packets.
	 *
	 * Logging identifies the origin codec or track index; the supplied
	 * logger controls throttling.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Remuxer final: public Step {
		friend class Backend::Pipeline::Detail::Worker::Remux;
		friend Remuxer& operator>>(Demuxer& demuxer, Remuxer& remuxer) noexcept;
		friend Remuxer& operator>>(Remuxer& remuxer, Muxer& muxer) noexcept;
		friend class Route;

		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Remuxer for origin stream @p in.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param in Origin stream index.
			 */
			explicit Remuxer(StormByte::Safe::Shared<StormByte::Logger::Log> log, int in) noexcept;

			/**
			 * @brief Copy constructor.
			 * @param other Source remuxer.
			 */
			Remuxer(const Remuxer& other) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Remuxer to take.
			 */
			Remuxer(Remuxer&& other) noexcept = delete;

			/**
			 * @brief Destructor. @ref Step::Join Halt s before backends die.
			 */
			~Remuxer() noexcept override;

			/**
			 * @brief Copy assignment.
			 * @param other Source remuxer.
			 * @return *this.
			 */
			Remuxer& operator=(const Remuxer& other) = delete;

			/**
			 * @brief Move assignment.
			 * @param other Remuxer to take.
			 * @return *this.
			 */
			Remuxer& operator=(Remuxer&& other) noexcept = delete;

			/**
			 * @}
			 */

			/**
			 * @brief Origin stream index.
			 * @return Index passed to the constructor.
			 */
			inline int In() const noexcept {
				return m_index;
			}

			/**
			 * @brief Maximum number of queued input packets.
			 * @return Packet limit, or `0` if no input queue exists.
			 */
			std::size_t InputCeiling() const noexcept override;

		private:
			/**
			 * @name Logging
			 * @{
			 */

			using Step::Log;

			/**
			 * @brief Token after `STMM ` for this remuxer.
			 * @return `Remuxer(<origin codec name>)` when the Plan
			 *         lists @ref In, otherwise `Remuxer(t=<index>)`.
			 */
			StormByte::Safe::String Label() const noexcept override;

			/**
			 * @}
			 */

			/**
			 * @brief @ref Step::Emit of the forwarded packet.
			 * @param packet Forwarded unit. Empty pointers are ignored.
			 */
			void Emit(Packet::PointerType packet) noexcept;

			int m_index;								///< Origin stream index
			Join m_join{*this};							///< Halt before other members die
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Remuxer);

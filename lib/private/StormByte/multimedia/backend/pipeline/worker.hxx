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

#include <StormByte/multimedia/backend/pipeline/host.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/visibility.h>

#include <string_view>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @class Worker
	 * @brief Stage body without a thread.
	 *
	 * A Pumper calls @ref Setup once and @ref Process for each
	 * unit. The Worker emits 0..N results through @ref Emit
	 * before returning. It has no State: on error it calls
	 * @ref Fail, which goes to the Host (Step or Filter::FFmpeg).
	 *
	 * Concretes live under Detail::Worker (Demux, Decode,
	 * Encode, Remux, Mux, Filter). Hoppers belong to the owner.
	 * No friends.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Worker {
		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			Worker(const Worker&) = delete;
			Worker(Worker&&) noexcept = delete;
			virtual ~Worker() noexcept = default;
			Worker& operator=(const Worker&) = delete;
			Worker& operator=(Worker&&) noexcept = delete;

			/**
			 * @}
			 */

			/**
			 * @brief One-shot backend bring-up.
			 *
			 * Must be noexcept. Called once by the Pumper. On
			 * error call @ref Fail and return. The Pumper sets
			 * Ready only if it is still Created after return.
			 */
			virtual void Setup() noexcept = 0;

			/**
			 * @brief One unit of the stage.
			 * @param item Popped unit, or empty for a source tick
			 *        or an EoF flush.
			 *
			 * Emit 0..N times, then return. On EoF the concrete
			 * calls @ref Flush itself; the Pumper does not.
			 */
			virtual void Process(Multimedia::Pipeline::Item::PointerType item) noexcept = 0;

		protected:
			/**
			 * @brief Worker bound to @p host.
			 * @param host Owner surface (Step or Filter::FFmpeg).
			 */
			explicit Worker(Host& host) noexcept;

			/**
			 * @brief Stage-specific drain when this worker sees EoF.
			 *
			 * Not called by the Pumper. Demux, mux and encode each
			 * implement their own flush. A filter that has nothing
			 * to drain implements an empty body.
			 */
			virtual void Flush() noexcept = 0;

			/**
			 * @brief Forwards @p item to the Host. Empty is a no-op.
			 * @param item Unit to emit. Ownership moves.
			 */
			void Emit(Multimedia::Pipeline::Item::PointerType item) noexcept;

			/**
			 * @brief Forwards Wait to the Host.
			 */
			void Wait() noexcept;

			/**
			 * @brief Forwards Stopping to the Host.
			 * @return true if the stage must return.
			 */
			bool Stopping() const noexcept;

			/**
			 * @brief Forwards Fail to the Host.
			 * @param reason Message.
			 */
			void Fail(std::string_view reason) noexcept;

			/**
			 * @brief Forwards Log to the Host.
			 * @param level StormByte::Logger::Level of this line.
			 * @param message Already-formatted text.
			 */
			void Log(StormByte::Logger::Level level, std::string_view message) noexcept;

			/**
			 * @brief Forwards Ended to the Host (source EoF).
			 */
			void Ended() noexcept;

		private:
			Host& m_host;	///< Owner, via Host
	};
}

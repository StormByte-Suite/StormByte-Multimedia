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
#include <StormByte/multimedia/backend/pipeline/worker.hxx>
#include <StormByte/multimedia/pipeline/typedefs.hxx>
#include <StormByte/safe/atomic.hxx>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/thread.hxx>
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
	 * @class Pumper
	 * @brief Thread and lifecycle of one stage.
	 *
	 * Owns the only @ref StormByte::Multimedia::Pipeline::State
	 * of the stage. Does not implement @ref Host and does not
	 * touch hoppers: the loop talks to a Host& (Step or
	 * Filter::FFmpeg, via Surface). Concretes live under
	 * Detail::Pumper (Source, Through, Sink).
	 *
	 * Bind / Launch / Halt / Stop are public on this private
	 * type so both Step and Filter::FFmpeg can compose a
	 * Pumper. No friends. Leaves never friend a Pumper or a
	 * Worker.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Pumper {
		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			Pumper(const Pumper&) = delete;
			Pumper(Pumper&&) noexcept = delete;
			virtual ~Pumper() noexcept;
			Pumper& operator=(const Pumper&) = delete;
			Pumper& operator=(Pumper&&) noexcept = delete;

			/**
			 * @}
			 */

			/**
			 * @brief Takes ownership of @p worker.
			 * @param worker Body. Must not be empty. No-op if a
			 *        worker is already bound or the thread runs.
			 */
			void Bind(StormByte::Safe::Unique<Worker> worker) noexcept;

			/**
			 * @brief Starts the thread: Setup, Ready, Pump.
			 *
			 * Idempotent. No-op without a bound worker.
			 */
			void Launch() noexcept;

			/**
			 * @brief Stop and join the thread.
			 *
			 * Safe to call more than once.
			 */
			void Halt() noexcept;

			/**
			 * @brief Asks the thread to leave.
			 *
			 * Idempotent. Does not join. Does not close hoppers;
			 * the owner does that.
			 */
			void Stop() noexcept;

			/**
			 * @brief Current lifecycle value.
			 * @return State of this pumper only.
			 */
			Multimedia::Pipeline::State Status() const noexcept;

			/**
			 * @brief Failure text.
			 * @return Message, or empty.
			 */
			const StormByte::Safe::Optional<StormByte::Safe::String>& Error() const noexcept;

			/**
			 * @brief Latches Failed. Does not close hoppers.
			 * @param reason Message.
			 */
			void Fail(std::string_view reason) noexcept;

		protected:
			/**
			 * @brief Pumper in State::Created, bound to @p host.
			 * @param host Owner surface (Step or Filter::FFmpeg).
			 */
			explicit Pumper(Host& host) noexcept;

			/**
			 * @brief Loop body. Source, Through and Sink override.
			 */
			virtual void Pump() noexcept = 0;

			/**
			 * @brief Source loop: Process({}) until Exhausted, Fail or Stop.
			 */
			void PumpSource() noexcept;

			/**
			 * @brief Through / sink loop: Pull, Process, flush on input EoF.
			 */
			void PumpPop() noexcept;

			/**
			 * @brief Bound worker, if any.
			 * @return Pointer, or null.
			 */
			Worker* Bound() noexcept;

			/**
			 * @brief Bound worker, if any.
			 * @return Pointer, or null.
			 */
			const Worker* Bound() const noexcept;

			/**
			 * @brief Owner surface.
			 * @return Host passed at construction.
			 */
			Host& Owner() noexcept;

			/**
			 * @brief Owner surface.
			 * @return Host passed at construction.
			 */
			const Host& Owner() const noexcept;

			/**
			 * @brief Whether the Pumper must return.
			 * @return true if Status is Stopping, Stopped or Failed.
			 */
			bool Stopping() const noexcept;

			/**
			 * @brief Whether this pumper has failed.
			 * @return true iff Status is Failed.
			 */
			bool Failed() const noexcept;

		private:
			Host& m_host;											///< Owner, via Host
			StormByte::Safe::Unique<Worker> m_worker;		///< Body
			StormByte::Safe::Atomic<int> m_state;			///< Lifecycle
			StormByte::Safe::Optional<StormByte::Safe::String> m_error;	///< Fail message
			StormByte::Safe::Thread m_thread;			///< Owned thread
	};
}

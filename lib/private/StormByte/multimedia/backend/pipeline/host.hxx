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

#include <StormByte/logger/log.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/pipeline/telemetry.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstdint>
#include <chrono>
#include <string_view>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @class Host
	 * @brief Owner surface that a Worker and a Pumper may call.
	 *
	 * Implemented by a private nested Surface of
	 * @ref StormByte::Multimedia::Pipeline::Step and of
	 * @ref StormByte::Multimedia::Pipeline::Filter::FFmpeg.
	 * Hoppers and logging stay on the owner. The Pumper is
	 * not a Host: it holds a Host&. No friends on this type.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Host {
		public:
			virtual ~Host() noexcept = default;

			Host(const Host&) = delete;
			Host(Host&&) noexcept = delete;
			Host& operator=(const Host&) = delete;
			Host& operator=(Host&&) noexcept = delete;

			/**
			 * @brief Hands a produced unit to the owner.
			 * @param item Unit to emit. Empty is a no-op.
			 *
			 * Ownership moves. The Worker must not use @p item
			 * after this call. A sink Worker does not call this.
			 */
			virtual void Emit(Multimedia::Pipeline::Item::PointerType item) noexcept = 0;

			/**
			 * @brief Blocks until hopper Ready, Failed or Stopping.
			 */
			virtual void Wait() noexcept = 0;

			/**
			 * @brief Whether the stage must return.
			 * @return true if Status is Stopping, Stopped or Failed.
			 */
			virtual bool Stopping() const noexcept = 0;

			/**
			 * @brief Marks Failed, closes hoppers and wakes waiters.
			 * @param reason Message.
			 *
			 * Does not join the thread. The implementer also
			 * latches the Pumper.
			 */
			virtual void Fail(std::string_view reason) noexcept = 0;

			/**
			 * @brief Writes one log line. No-op if the owner has no logger.
			 * @param level StormByte::Logger::Level of this line.
			 * @param message Already-formatted text.
			 */
			virtual void Log(StormByte::Logger::Level level, std::string_view message) noexcept = 0;

			/**
			 * @brief Source has no more units. No-op on through and sink.
			 *
			 * Demux calls this when Read is empty. The Source
			 * pumper leaves the loop.
			 */
			virtual void Ended() noexcept = 0;

			/**
			 * @brief Whether @ref Ended was called.
			 * @return true after source EoF.
			 */
			virtual bool Exhausted() const noexcept = 0;

			/**
			 * @brief Next unit from the input hopper.
			 * @return Item, or empty if none is ready.
			 */
			virtual Multimedia::Pipeline::Item::PointerType Pull() noexcept = 0;

			/**
			 * @brief Whether the input hopper is EoF.
			 * @return true when Pull will not produce more units.
			 */
			virtual bool InputEof() const noexcept = 0;

			/**
			 * @brief Eof on Out and CloneTo forks after Pump returns.
			 *
			 * Forks must close or look / analytics Wait forever.
			 */
			virtual void CloseOutput() noexcept = 0;

			/**
			 * @brief Pumper has CAS Created → Ready. Wake waiters.
			 */
			virtual void BecameReady() noexcept = 0;

			/**
			 * @brief Adds one Process duration to the step summary.
			 * @param microseconds Wall time of that Process call.
			 */
			virtual void RecordWork(std::chrono::nanoseconds duration) noexcept = 0;

			/**
			 * @brief Stage counters updated by the worker thread.
			 * @return Mutable stage metrics.
			 */
			virtual Multimedia::Pipeline::StageTelemetry& Telemetry() noexcept = 0;

			/**
			 * @brief Writes min/max Process time at Debug. No-op if none.
			 */
			virtual void DumpWork() noexcept = 0;

		protected:
			Host() noexcept = default;
	};
}

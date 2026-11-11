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

#include <StormByte/multimedia/pipeline/telemetry.hxx>

namespace StormByte::Multimedia::Backend::Pipeline {
	using StormByte::Multimedia::Pipeline::ActivityState;
	using StormByte::Multimedia::Pipeline::WorkerActivity;

	/**
	 * @brief Worker-local instrumentation context, never stored in retained snapshots.
	 */
	inline thread_local WorkerActivity* CurrentActivity = nullptr;

	/**
	 * @brief Bind a thread to an independently retained activity clock.
	 */
	class ActivityBinding final {
		public:
			/**
			 * @brief Start the worker clock and publish it to nested instrumentation.
			 * @param worker Clock or null when registration was unavailable.
			 * @param stage Owning stage, alive until this binding is destroyed.
			 */
			ActivityBinding(WorkerActivity* worker, const StormByte::Multimedia::Pipeline::StageTelemetry& stage) noexcept
			: m_worker(worker), m_stage(stage), m_previous(CurrentActivity) {
				CurrentActivity = worker;
				if (worker)
					worker->Start();
			}

			ActivityBinding(const ActivityBinding&) = delete;

			ActivityBinding(ActivityBinding&&) noexcept = delete;

			/**
			 * @brief Freeze the clock and restore the previous thread-local context.
			 */
			~ActivityBinding() noexcept {
				if (m_worker)
					m_worker->Finish(m_stage.Status() == StormByte::Multimedia::Pipeline::State::Failed);
				CurrentActivity = m_previous;
			}

			ActivityBinding& operator=(const ActivityBinding&) = delete;

			ActivityBinding& operator=(ActivityBinding&&) noexcept = delete;

		private:
			WorkerActivity* m_worker; ///< Clock owned by the stage or feeder.
			const StormByte::Multimedia::Pipeline::StageTelemetry& m_stage; ///< Terminal failure source.
			WorkerActivity* m_previous; ///< Outer thread-local context.
	};

	/**
	 * @brief Exclusive nested category; restores the previous one at scope exit.
	 */
	class ActivityScope final {
		public:
			/**
			 * @brief Suspend the previous category without double-counting it.
			 * @param state New nonterminal category.
			 */
			explicit ActivityScope(ActivityState state) noexcept
			: m_worker(CurrentActivity), m_previous(m_worker ? m_worker->Exchange(state) : ActivityState::Unknown) {}

			ActivityScope(const ActivityScope&) = delete;

			ActivityScope(ActivityScope&&) noexcept = delete;

			/**
			 * @brief Restore the outer category unless execution has already ended.
			 */
			~ActivityScope() noexcept {
				if (m_worker)
					m_worker->Exchange(m_previous);
			}

			ActivityScope& operator=(const ActivityScope&) = delete;

			ActivityScope& operator=(ActivityScope&&) noexcept = delete;

		private:
			WorkerActivity* m_worker; ///< Borrowed only for the instrumented scope.
			ActivityState m_previous; ///< Suspended category.
	};
}

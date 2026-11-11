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

#include <StormByte/multimedia/backend/pipeline/queue_probe.hxx>
#include <StormByte/multimedia/pipeline/telemetry.hxx>

#include <optional>
#include <vector>

namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @brief Registration record for one real pipeline stage.
	 */
	struct STORMBYTE_MULTIMEDIA_PRIVATE TopologyDescriptor {
		StormByte::Multimedia::Pipeline::TopologyStageKind Kind = StormByte::Multimedia::Pipeline::TopologyStageKind::Demuxer;
		StormByte::Multimedia::Pipeline::TopologyFilterKind Filter = StormByte::Multimedia::Pipeline::TopologyFilterKind::None;
		StormByte::Safe::Optional<int> Track;
		StormByte::Safe::Optional<int> OutputTrack;
		StormByte::Safe::Optional<enum StormByte::Multimedia::Type> Media;
		StormByte::Safe::Optional<std::uint32_t> FilterPosition;
		StormByte::Safe::String FilterName;
		StormByte::Safe::String Name;
		StormByte::Safe::Shared<const StormByte::Multimedia::Pipeline::StageTelemetry> Metrics;
		StormByte::Safe::Shared<QueueProbe> Probe;	///< Never the stage itself.
	};

	/**
	 * @class Topology
	 * @brief Thread-safe recorder of one job's stages and queues.
	 *
	 * Holds only stage metrics and queue probes. @ref Freeze converts the
	 * live view into a final copy and drops the probes and budget handles.
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Topology {
		public:
			Topology() noexcept = default;
			Topology(const Topology&) = delete;
			Topology& operator=(const Topology&) = delete;

			/**
			 * @brief Start a new run with no stages.
			 */
			void Reset() noexcept;

			/**
			 * @brief Start a new run anchoring the job budget first.
			 *
			 * The budget is identified before Reset releases the previous
			 * run's anchors, so a budget closed later stays identifiable.
			 * A lost anchor marks the run incomplete.
			 * @param budget Job budget.
			 */
			void Begin(const StormByte::Buffer::ByteBudget& budget) noexcept;

			/**
			 * @brief Register one stage once per probe.
			 * @param stage Descriptor.
			 * @throw Safe::AllocationError or std::bad_alloc.
			 */
			void Register(TopologyDescriptor stage);

			/**
			 * @brief Capture a final copy and release probes.
			 *
			 * If the copy cannot be allocated, later snapshots report
			 * TopologySnapshotState::Unfrozen and Complete == false instead
			 * of a Final view whose shared budgets could still change.
			 */
			void Freeze() noexcept;

			/**
			 * @brief Keep @p identity alive for this run so its budget stays
			 *        identifiable even if it is closed before queues register.
			 * @param identity Budget identity.
			 * @throw Safe::AllocationError or std::bad_alloc.
			 */
			void Pin(StormByte::Safe::Shared<BudgetIdentity> identity);

			/**
			 * @brief Record that a registration was lost to an allocation failure.
			 */
			void MarkIncomplete() noexcept;

			/**
			 * @brief Current or final view.
			 * @return Owned snapshot.
			 * @throw Safe::AllocationError or std::bad_alloc.
			 */
			StormByte::Multimedia::Pipeline::TopologySnapshot Snapshot() const;

		private:
			StormByte::Multimedia::Pipeline::TopologySnapshot Build(StormByte::Multimedia::Pipeline::TopologySnapshotState state) const;

			mutable StormByte::Safe::Mutex m_mutex;										///< Guards all fields.
			std::vector<TopologyDescriptor> m_stages;									///< Live registrations.
			std::vector<StormByte::Safe::Shared<BudgetIdentity>> m_budgets;				///< Identities kept stable after freeze.
			std::optional<StormByte::Multimedia::Pipeline::TopologySnapshot> m_final;	///< Frozen view.
			std::uint64_t m_run = 0;													///< Run generation.
			bool m_available = false;													///< A run registered stages.
			bool m_frozen = false;														///< Stages torn down, even if the final copy failed.
			bool m_incomplete = false;													///< A registration was lost.
	};
}

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

#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/map.hxx>
#include <StormByte/safe/string.hxx>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter
 * @brief Frame and packet steps attached to a job or a raw pipeline.
 */
namespace StormByte::Multimedia::Pipeline::Filter {
	/**
	 * @class Report
	 * @brief Optional measurement produced by a filter.
	 *
	 * Status is measurement-only. Quality thresholds belong in
	 * @c Transcode::OnReport and @c Transcode::ExtraData, not here.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::FFmpeg
	 * @note Requires a compatible C++ ABI. Base owns payload storage; all
	 * participating providers must remain loaded through destruction of reports,
	 * copied payloads and borrowed views. Special members execute in Multimedia.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Report {
		public:
			/**
			 * @enum Status
			 * @brief Whether this node produced usable data.
			 */
			enum class Status {
				None,		///< Filter does not report.
				Ok,		///< Data() is usable.
				Failed		///< Measurement could not be taken.
			};

			/**
			 * @brief Empty report (`None`).
			 */
			Report();

			/**
			 * @brief Report with a status and a dictionary.
			 * @param status Measurement result.
			 * @param data Key/value payload (owned).
			 */
			Report(Status status, StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> data) noexcept;

			/**
			 * @brief Copy constructor.
			 * @param other Source report.
			 */
			Report(const Report& other);

			/**
			 * @brief Move constructor.
			 * @param other Source report.
			 */
			Report(Report&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			~Report() noexcept;

			/**
			 * @brief Copy assignment.
			 * @param other Source report.
			 * @return *this.
			 */
			Report& operator=(const Report& other);

			/**
			 * @brief Move assignment.
			 * @param other Source report.
			 * @return *this.
			 */
			Report& operator=(Report&& other) noexcept;

			/**
			 * @brief Measurement status.
			 * @return Status value.
			 */
			Status Kind() const noexcept;

			/**
			 * @brief Dictionary payload.
			 * @return Borrowed key/value map, valid until report mutation or destruction.
			 */
			const StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String>& Data() const noexcept;

			/**
			 * @brief Single callable dump of status plus data.
			 * @return Human-readable snapshot.
			 */
			StormByte::Safe::String operator()() const;

		private:
			Status m_status;	///< Measurement status
			StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> m_data;	///< Base-owned payload with creator-dispatched node lifetime
	};
}

/**
 * @brief Report requires compatible C++ ABI and loaded Multimedia and Base modules.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Report);

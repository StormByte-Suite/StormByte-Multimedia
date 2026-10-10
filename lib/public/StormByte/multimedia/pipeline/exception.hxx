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

#include <StormByte/multimedia/exception.hxx>
#include <StormByte/multimedia/visibility.h>

#include <format>
#include <string_view>
#include <utility>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Public Multimedia module.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Pipeline
		 * @brief Demux / decode / filter / encode / mux types.
		 *
		 * @ingroup multimedia_pipeline
		 */
		namespace Pipeline {
			/**
			 * @class PlanException
			 * @brief Thrown when @ref Plan::Check finds a malformed plan.
			 *
			 * Formation only (missing origin track, type mismatch, empty
			 * path). Not an FFmpeg or Step failure.
			 *
			 * @ingroup multimedia_pipeline
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC PlanException: public StormByte::Multimedia::Exception {
				public:
					/**
					 * @brief Constructs a formatted Plan exception.
					 * @tparam Args Format argument types.
					 * @param fmt Format string.
					 * @param args Format arguments.
					 */
					template<typename... Args>
					PlanException(std::format_string<Args...> fmt, Args&&... args):
						StormByte::Multimedia::Exception("Plan", fmt, std::forward<Args>(args)...) {}

					/**
					 * @brief Constructs a Plan exception from a message.
					 * @param message Exception message.
					 */
					explicit PlanException(std::string_view message):
						StormByte::Multimedia::Exception("Plan", "{}", message) {}

					/**
					 * @brief Copy constructor.
					 * @param other Exception to copy.
					 */
					PlanException(const PlanException& other);

					/**
					 * @brief Move constructor.
					 * @param other Exception to take.
					 */
					PlanException(PlanException&& other) noexcept;

					/**
					 * @brief Destructor. Defined in the Multimedia library to anchor RTTI.
					 */
					~PlanException() noexcept override;

					/**
					 * @brief Copy assignment.
					 * @param other Exception to copy.
					 * @return *this.
					 */
					PlanException& operator=(const PlanException& other);

					/**
					 * @brief Move assignment.
					 * @param other Exception to take.
					 * @return *this.
					 */
					PlanException& operator=(PlanException&& other) noexcept;
			};
		}
	}
}

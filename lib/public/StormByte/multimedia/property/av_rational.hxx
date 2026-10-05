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

#include <StormByte/multimedia/visibility.h>
#include <StormByte/type_traits/safe.hxx>

#include <cstdint>

/**
 * @namespace StormByte::Multimedia::Property
 * @brief Media property value types.
 */
namespace StormByte::Multimedia::Property {
	/**
	 * @class AVRational
	 * @brief Pair `{num, den}` matching libavutil `AVRational`.
	 *
	 * Public fields so `fps.num`, `fps.den` and `AVRational{24000, 1001}`
	 * read like the C API. This is not libav's header: plugins that
	 * include `libavutil/rational.h` still use `::AVRational` for the
	 * C struct. Convert with `{r.num, r.den}`.
	 *
	 * 23.976 fps is `{24000, 1001}`. 24 fps is `{24, 1}`. A time base
	 * of 1 ms is `{1, 1000}`.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC AVRational {
		public:
			int num = 0;	///< Numerator (`AVRational.num`), initialized to zero.

			int den = 1;	///< Denominator (`AVRational.den`), initialized to one.

			/**
			 * @brief `{0, 1}` — unknown / unset.
			 */
			constexpr AVRational() noexcept = default;

			/**
			 * @brief `{num, den}` like the C aggregate.
			 * @param num Numerator.
			 * @param den Denominator.
			 */
			constexpr AVRational(int num, int den) noexcept
			: num(num), den(den) {}

			/**
			 * @brief True when both sides are positive.
			 * @return true if `num > 0 && den > 0`.
			 */
			constexpr bool Valid() const noexcept {
				return num > 0 && den > 0;
			}

			/**
			 * @brief `av_q2d` — `num / den` as double.
			 * @return Quotient, or 0 if `den == 0`.
			 */
			constexpr double ToDouble() const noexcept {
				return den ? static_cast<double>(num) / static_cast<double>(den) : 0.0;
			}

			/**
			 * @brief `av_rescale_q(ticks, *this, dst)`.
			 * @param ticks Source ticks.
			 * @param dst Destination time base.
			 * @return Scaled ticks, or `AV_NOPTS_VALUE` when either side is invalid.
			 */
			std::int64_t Rescale(std::int64_t ticks, const AVRational& dst) const noexcept;

			/**
			 * @brief Equality.
			 * @param other Other rational.
			 * @return true if num and den match.
			 */
			constexpr bool operator==(const AVRational& other) const noexcept {
				return num == other.num && den == other.den;
			}

			/**
			 * @brief Inequality.
			 * @param other Other rational.
			 * @return true if num or den differ.
			 */
			constexpr bool operator!=(const AVRational& other) const noexcept {
				return !(*this == other);
			}
	};
}

/**
 * @brief Registers the completed rational for Safe value storage.
 *
 * Integer numerator and denominator own no heap storage; value lifetime operations
 * allocate nothing. Compatible provider ABI and provider lifetime are required.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Property::AVRational);

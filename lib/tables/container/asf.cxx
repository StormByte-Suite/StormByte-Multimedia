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

#include <tables/container/table.hxx>

using namespace StormByte::Multimedia::Tables::Container;

namespace {
	constexpr CompatDef table[] = {
		{ nullptr,	"MS MPEG-4 v3" },
		{ nullptr,	"VC-1" },
		{ nullptr,	"WMV 7" },
		{ nullptr,	"WMV 8" },
		{ nullptr,	"WMV 9" },
		{ nullptr,	"WMV 9 Image" },

		{ nullptr,	"MP3" },
		{ nullptr,	"WMA Lossless" },
		{ nullptr,	"WMA Pro" },
		{ nullptr,	"WMA Voice" },
		{ nullptr,	"WMA v1" },
		{ nullptr,	"WMA v2" },

		{ "wmv",	"MS MPEG-4 v3" },
		{ "wmv",	"VC-1" },
		{ "wmv",	"WMV 7" },
		{ "wmv",	"WMV 8" },
		{ "wmv",	"WMV 9" },
		{ "wmv",	"WMA Lossless" },
		{ "wmv",	"WMA Pro" },
		{ "wmv",	"WMA v1" },
		{ "wmv",	"WMA v2" },
		{ "wmv",	"MP3" },

		{ "wma",	"WMA Lossless" },
		{ "wma",	"WMA Pro" },
		{ "wma",	"WMA Voice" },
		{ "wma",	"WMA v1" },
		{ "wma",	"WMA v2" },
	};
}

std::span<const CompatDef> StormByte::Multimedia::Tables::Container::ASF() noexcept {
	return table;
}

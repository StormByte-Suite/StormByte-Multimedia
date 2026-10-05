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

#include <StormByte/multimedia/log.hxx>

#include <StormByte/logger/log.hxx>
#include <StormByte/logger/manipulators.hxx>
#include <StormByte/safe/pointers.hxx>

#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace StormByte::Multimedia {
	namespace {
		std::once_flag g_moduleOnce;
		std::mutex g_moduleMutex;
		StormByte::Safe::Shared<StormByte::Logger::Log> g_module;

		void ConfigureModule(StormByte::Logger::Log& mm) {
			mm.Format("[%L] %T %c");

			// StormByte::Logger::ThrottleSpec low;
			// low.Level = StormByte::Logger::Level::LowLevel;
			// low.Rate = 8.0;
			// low.Burst = 16;
			// low.Policy = StormByte::Logger::ThrottlePolicy::Window;
			// low.WindowKeep = 12;
			// low.WindowPeriod = 16;
			// mm.Throttle(low);

			// mm.Throttle(StormByte::Logger::Level::Debug, 2.0, 4);
			// mm.Throttle(StormByte::Logger::Level::Notice, 4.0, 8);
		}

		StormByte::Safe::Shared<StormByte::Logger::Log> ModuleRoot(
			StormByte::Safe::Shared<StormByte::Logger::Log> log) {
			std::lock_guard<std::mutex> lock(g_moduleMutex);
			if (!g_module)
				g_module = log->Scope("StormByte/Multimedia");
			return g_module;
		}
	}

	StormByte::Safe::Shared<StormByte::Logger::Log> UseLog(
		StormByte::Safe::Shared<StormByte::Logger::Log> log, std::string_view leaf) noexcept {
		if (!log)
			return {};
		auto mm = ModuleRoot(std::move(log));
		std::call_once(g_moduleOnce, [&]() { ConfigureModule(*mm); });
		if (leaf.empty())
			return mm;
		return mm->Scope(std::string(leaf));
	}
}

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

#include <StormByte/multimedia/log.hxx>

#include <StormByte/logger/log.hxx>
#include <StormByte/logger/manipulators.hxx>
#include <StormByte/safe/pointers.hxx>

#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <algorithm>
#include <vector>

namespace StormByte::Multimedia {
	namespace {
		std::mutex g_moduleMutex;
		struct ModuleEntry {
			std::vector<StormByte::Safe::Weak<StormByte::Logger::Log>> Facades;
			StormByte::Safe::Shared<StormByte::Logger::Log> Module;
		};
		std::vector<ModuleEntry> g_modules;

		void ConfigureModule(StormByte::Logger::Log& mm) {
			mm.Format("[%L] %T %c");

			StormByte::Logger::ThrottleSpec low;
			low.Level = StormByte::Logger::Level::LowLevel;
			low.Rate = 8.0;
			low.Burst = 16;
			low.Policy = StormByte::Logger::ThrottlePolicy::Window;
			low.WindowKeep = 12;
			low.WindowPeriod = 16;
			mm.Throttle(low);

			mm.Throttle(StormByte::Logger::Level::Debug, 2.0, 4);
			mm.Throttle(StormByte::Logger::Level::Notice, 4.0, 8);
		}

	}

	StormByte::Safe::Shared<StormByte::Logger::Log> UseLog(
		StormByte::Safe::Shared<StormByte::Logger::Log> log, std::string_view leaf) noexcept {
		if (!log)
			return {};
		std::lock_guard<std::mutex> lock(g_moduleMutex);
		for (auto& entry : g_modules)
			std::erase_if(entry.Facades, [](const auto& facade) { return facade.expired(); });
		std::erase_if(g_modules, [](const auto& entry) {
			return entry.Facades.empty() && entry.Module.use_count() == 1;
		});
		ModuleEntry* module = nullptr;
		for (auto& entry : g_modules) {
			if (entry.Module.get() == log.get() || std::any_of(entry.Facades.begin(), entry.Facades.end(),
				[&log](const auto& facade) { return facade.lock().get() == log.get(); })) {
				module = &entry;
				break;
			}
		}
		if (!module) {
			auto mm = log->Scope("StormByte/Multimedia");
			ConfigureModule(*mm);
			g_modules.push_back({{StormByte::Safe::Weak<StormByte::Logger::Log>{log}}, std::move(mm)});
			module = &g_modules.back();
		}
		if (leaf.empty())
			return module->Module;
		auto scoped = module->Module->Scope(std::string(leaf));
		module->Facades.emplace_back(scoped);
		return scoped;
	}
}

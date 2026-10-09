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

#include <StormByte/safe/string.hxx>

#include <filesystem>
#include <iostream>
#include <span>
#include <streambuf>
#include <string_view>

/** @brief CTest return code for an unavailable optional encoder case. */
inline constexpr int TEST_SKIP = 77;

/** @brief Named test function accepted by the per-function CTest runner. */
using TestFunction = int (*)();

/** @brief One independently invocable test case. */
struct TestEntry {
	std::string_view name; ///< Stable test-function identifier passed by CTest.
	TestFunction function; ///< Function body for the test case.
};

/** @brief Output buffer that reports every write as consumed without storing bytes. */
class DiscardStreamBuffer final: public std::streambuf {
	public:
		/** @brief Accept one character without forwarding it. */
		int_type overflow(int_type character) override {
			return traits_type::not_eof(character);
		}

		/** @brief Accept a character range without forwarding it. */
		std::streamsize xsputn(const char*, std::streamsize count) override {
			return count;
		}
};

/** @brief Temporarily diverts std::cout to a discard buffer and restores it on scope exit. */
class QuietCout final {
	public:
		/** @brief Installs the discard buffer and remembers the original stream buffer. */
		QuietCout() noexcept: m_original(std::cout.rdbuf(&m_discard)) {}

		/** @brief Restores the stream buffer saved at construction. */
		~QuietCout() noexcept {
			std::cout.rdbuf(m_original);
		}

		/** @brief Copying would make stream-buffer restoration ambiguous. */
		QuietCout(const QuietCout&) = delete;

		/** @brief Copy assignment is disabled. */
		QuietCout& operator=(const QuietCout&) = delete;

	private:
		DiscardStreamBuffer m_discard; ///< Sink installed while the guard is alive.
		std::streambuf* m_original; ///< Stream buffer restored at destruction.
};

/** @brief Logs lifecycle around one function so a timeout identifies the active case. */
inline int RunOneTest(const TestEntry& test) {
	std::cout << "[BEGIN] " << test.name << std::endl;
	const int result = test.function();
	if (result == TEST_SKIP) {
		std::cout << "[END] " << test.name << " SKIP" << std::endl;
		return TEST_SKIP;
	}
	std::cout << "[END] " << test.name << (result == 0 ? " PASS" : " FAIL") << std::endl;
	return result;
}

/** @brief Runs one selected case or every case when no name is passed. */
inline int RunSelectedTest(int argc, char** argv, std::span<const TestEntry> tests) {
	if (argc == 1) {
		int failures = 0;
		for (const auto& test : tests) {
			const int result = RunOneTest(test);
			if (result != 0 && result != TEST_SKIP)
				++failures;
		}
		return failures;
	}

	const std::string_view selected{argv[1]};
	for (const auto& test : tests) {
		if (test.name == selected)
			return RunOneTest(test);
	}

	std::cerr << "Unknown test function: " << selected << std::endl;
	return 2;
}

/** @brief Resolves a fixture under the test/files directory. */
inline std::filesystem::path FixturePath(std::string_view relativePath) {
	return std::filesystem::path{STORMBYTE_TEST_FILES_DIR} / relativePath;
}

/** @brief Resolves a generated result path and creates its parent directory. */
inline std::filesystem::path OutputPath(std::string_view relativePath) {
	auto path = std::filesystem::path{STORMBYTE_TEST_OUTPUT_DIR} / relativePath;
	std::error_code error;
	std::filesystem::create_directories(path.parent_path(), error);
	if (error && !std::filesystem::is_directory(path.parent_path())) {
		std::cerr << "[ASSERT] OutputPath: " << error.message() << std::endl;
		return {};
	}
	return path;
}

/** @brief Returns a view of a Base-owned UTF-8 string for fixed-value assertions. */
inline std::string_view TestView(const StormByte::Safe::String& value) {
	return {value.data(), static_cast<std::size_t>(value.size())};
}

#define TEST_REQUIRE(expression) \
	do { \
		if (!(expression)) { \
			std::cerr << "[ASSERT] " << __func__ << ": " << #expression << std::endl; \
			return 1; \
		} \
	} while (false)

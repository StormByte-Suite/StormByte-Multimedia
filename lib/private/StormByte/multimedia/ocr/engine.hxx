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

#include <StormByte/expected.hxx>
#include <StormByte/multimedia/ocr/exception.hxx>
#include <StormByte/safe/pointers.hxx>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

/**
 * @namespace StormByte::Multimedia::OCR
 * @brief Private Tesseract OCR helpers.
 */
namespace StormByte::Multimedia::OCR {
	/**
	 * @brief Recognized UTF-8 text, or an OCR / tessdata error.
	 */
	using ExpectedText = Expected<std::string, StormByte::Multimedia::Exception>;

	/**
	 * @class Engine
	 * @brief In-memory Tesseract session for bitmap subtitle cues.
	 *
	 * Language may be empty; Open then loads `eng`. The encoder only calls
	 * Recognize() and never touches Tesseract.
	 */
	class Engine {
		public:
			/**
			 * @name Construction
			 * @{
			 */

			/**
			 * @brief Constructs an engine with no language pin (falls back to `eng`).
			 */
			Engine() noexcept;

			/**
			 * @brief Constructs an engine pinned to @p language.
			 * @param language Tesseract / tessdata key (`spa`, `eng`, …). Empty uses `eng`.
			 */
			explicit Engine(std::string language) noexcept;

			/**
			 * @brief Move constructor.
			 * @param other Engine to take.
			 */
			Engine(Engine&& other) noexcept;

			/**
			 * @brief Copy constructor.
			 * @note Sessions are not copyable.
			 */
			Engine(const Engine&) = delete;

			/**
			 * @brief Destroys the session and ends Tesseract.
			 */
			~Engine() noexcept;

			/**
			 * @brief Move assignment.
			 * @param other Engine to take.
			 * @return *this.
			 */
			Engine& operator=(Engine&& other) noexcept;

			/**
			 * @brief Copy assignment.
			 * @note Sessions are not copyable.
			 * @return *this.
			 */
			Engine& operator=(const Engine&) = delete;

			/**
			 * @}
			 */

			/**
			 * @name Language
			 * @{
			 */

			/**
			 * @brief Pinned language key.
			 * @return Key as set by the caller. Empty means fallback `eng`.
			 */
			const std::string& Language() const noexcept;

			/**
			 * @brief Pins a language key. Reopens Tesseract on the next Recognize().
			 * @param language Tesseract / tessdata key. Empty means fallback `eng`.
			 */
			void Language(std::string language) noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Recognition
			 * @{
			 */

			/**
			 * @brief Runs OCR on an 8-bit grayscale bitmap.
			 * @param gray Packed pixels, at least stride * height bytes.
			 * @param width Image width in pixels.
			 * @param height Image height in pixels.
			 * @param stride Bytes per row (must be >= width).
			 * @return UTF-8 text, or OCRException / TessDataNotFoundException.
			 */
			ExpectedText Recognize(
				std::span<const std::uint8_t> gray,
				int width,
				int height,
				int stride) noexcept;

			/**
			 * @}
			 */

		private:
			/**
			 * @class Impl
			 * @brief Tesseract handle and last opened language.
			 */
			class Impl;

			StormByte::Safe::Unique<Impl> m_impl;	///< Base-owned backend session.
			std::string m_language;			///< Caller language pin; empty uses `eng`.
	};
}

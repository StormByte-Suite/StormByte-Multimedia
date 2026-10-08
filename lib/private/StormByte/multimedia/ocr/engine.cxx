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

#include <StormByte/multimedia/ocr/engine.hxx>
#include <StormByte/multimedia/ocr/tessdata.hxx>

#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>

#include <utility>

/**
 * @file engine.cxx
 * @brief Tesseract session for bitmap subtitle OCR.
 */

namespace StormByte::Multimedia::OCR {
	namespace {
		/**
		 * @brief tessdata key used when the stream has no language tag.
		 */
		constexpr std::string_view FallbackLanguage = "eng";
	}

	/**
	 * @class Engine::Impl
	 * @brief Owns TessBaseAPI and caches the last successful Init language.
	 */
	class Engine::Impl {
		public:
			/**
			 * @brief Constructs an unopened Tesseract handle.
			 */
			Impl() noexcept = default;

			/**
			 * @brief Ends Tesseract.
			 */
			~Impl() noexcept {
				m_api.End();
			}

			/**
			 * @brief Loads tessdata and inits Tesseract when the language changed.
			 * @param language Stream pin. Empty uses the default `eng` blob (no language pin).
			 * @return true if Tesseract is ready.
			 *
			 * A non-empty pin must load that exact tessdata. There is no silent
			 * fallback to another language when the requested model is missing.
			 */
			bool Open(std::string_view language) noexcept {
				const std::string key = language.empty()
					? std::string(FallbackLanguage)
					: std::string(language);
				if (m_open && m_openLanguage == key)
					return true;

				m_api.End();
				m_open = false;
				m_openLanguage.clear();

				auto blob = TessData(key);
				if (!blob.has_value())
					return false;

				const auto data = blob.value();
				const int rc = m_api.Init(
					reinterpret_cast<const char*>(data.data()),
					static_cast<int>(data.size()),
					key.c_str(),
					tesseract::OEM_DEFAULT,
					nullptr,
					0,
					nullptr,
					nullptr,
					false,
					nullptr);
				if (rc != 0)
					return false;

				m_api.SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);
				m_open = true;
				m_openLanguage = key;
				return true;
			}

			tesseract::TessBaseAPI m_api;	///< Tesseract API.
			std::string m_openLanguage;		///< Language passed to the last successful Init.
			bool m_open = false;			///< true after a successful Init.
	};

	Engine::Engine() noexcept
	: m_impl(StormByte::Safe::MakeUnique<Impl>()) {}

	Engine::Engine(std::string language) noexcept
	: m_impl(StormByte::Safe::MakeUnique<Impl>()), m_language(std::move(language)) {}

	Engine::Engine(Engine&&) noexcept = default;
	Engine::~Engine() noexcept = default;
	Engine& Engine::operator=(Engine&&) noexcept = default;

	const std::string& Engine::Language() const noexcept {
		return m_language;
	}

	void Engine::Language(std::string language) noexcept {
		if (m_language == language)
			return;
		m_language = std::move(language);
		if (m_impl)
			m_impl->m_open = false;
	}

	ExpectedText Engine::Recognize(
		std::span<const std::uint8_t> gray,
		int width,
		int height,
		int stride) noexcept {
		if (!m_impl)
			return Unexpected<OCRException>("OCR engine is not available");
		if (width <= 0 || height <= 0 || stride < width || gray.size() < static_cast<std::size_t>(stride * height))
			return Unexpected<OCRException>("invalid OCR image");
		if (!m_impl->Open(m_language)) {
			if (!m_language.empty())
				return Unexpected<TessDataNotFoundException>(m_language);
			return Unexpected<TessDataNotFoundException>(std::string(FallbackLanguage));
		}

		m_impl->m_api.SetImage(gray.data(), width, height, 1, stride);
		char* raw = m_impl->m_api.GetUTF8Text();
		if (!raw)
			return Unexpected<OCRException>("tesseract returned no text");
		std::string text(raw);
		delete[] raw;
		while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' '))
			text.pop_back();
		return text;
	}
}

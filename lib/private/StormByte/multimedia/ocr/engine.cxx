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

#include <StormByte/multimedia/ocr/engine.hxx>
#include <StormByte/multimedia/ocr/tessdata.hxx>

#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>

#include <utility>
#include <vector>
#include <algorithm>
#include <limits>
#ifdef STORMBYTE_MULTIMEDIA_TESSDATA_SYSTEM
	#include <filesystem>
#endif

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
	 * @class Engine::Model
	 * @brief Owns TessBaseAPI and caches the last successful Init language.
	 */
	class Engine::Model {
		public:
			/**
			 * @brief Supplies secondary language models from the embedded catalog.
			 * @param filename Tesseract model path, with either platform's separators.
			 * @param destination Receives the requested model bytes.
			 * @return true if the exact model was found and copied.
			 */
#ifndef STORMBYTE_MULTIMEDIA_TESSDATA_SYSTEM
			static bool Read(const char* filename, std::vector<char>* destination) noexcept {
				if (!filename || !destination)
					return false;
				try {
					std::string_view name{filename};
					const auto separator = name.find_last_of("/\\");
					if (separator != std::string_view::npos)
						name.remove_prefix(separator + 1);
					constexpr std::string_view extension = ".traineddata";
					if (!name.ends_with(extension))
						return false;
					name.remove_suffix(extension.size());
					const auto model = TessData(name);
					if (!model)
						return false;
					const auto bytes = *model;
					const auto* begin = reinterpret_cast<const char*>(bytes.data());
					destination->assign(begin, begin + bytes.size());
					return true;
				}
				catch (...) {
					return false;
				}
			}
#endif

			/**
			 * @brief Constructs an unopened Tesseract handle.
			 */
			Model() noexcept = default;

			/**
			 * @brief Ends Tesseract.
			 */
			~Model() noexcept {
				m_api.End();
			}

			/**
			 * @brief Loads tessdata and inits Tesseract when the language changed.
			 * @param language Stream pin. Empty selects `eng` from the configured model source.
			 * @return true if Tesseract is ready.
			 *
			 * A non-empty pin must load that exact tessdata. There is no silent
			 * fallback to another language when the requested model is missing.
			 */
			bool Open(std::string_view language) noexcept {
				const StormByte::Safe::String key = language.empty()
					? StormByte::Safe::String{FallbackLanguage}
					: StormByte::Safe::String{language};
				if (m_open && m_openLanguage == key)
					return true;

				m_api.End();
				m_open = false;
				m_openLanguage.clear();

#ifdef STORMBYTE_MULTIMEDIA_TESSDATA_SYSTEM
				std::error_code error;
				const auto path = std::filesystem::path{STORMBYTE_MULTIMEDIA_TESSDATA_DIR}
					/ (static_cast<std::string>(key) + ".traineddata");
				if (!std::filesystem::is_regular_file(path, error) || error)
					return false;
				const int rc = m_api.Init(STORMBYTE_MULTIMEDIA_TESSDATA_DIR, key.c_str(), tesseract::OEM_DEFAULT);
#else
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
					&Read);
#endif
				if (rc != 0)
					return false;

				m_api.SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);
				m_open = true;
				m_openLanguage = key;
				return true;
			}

			tesseract::TessBaseAPI m_api;	///< Tesseract API.
			StormByte::Safe::String m_openLanguage;	///< Language passed to the last successful Init.
			bool m_open = false;			///< true after a successful Init.
	};

	Engine::Engine() noexcept
	: m_model(StormByte::Safe::MakeUnique<Model>()) {}

	Engine::Engine(StormByte::Safe::String language) noexcept
	: m_model(StormByte::Safe::MakeUnique<Model>()), m_language(std::move(language)) {}

	Engine::Engine(Engine&&) noexcept = default;
	Engine::~Engine() noexcept = default;
	Engine& Engine::operator=(Engine&&) noexcept = default;

	const StormByte::Safe::String& Engine::Language() const noexcept {
		return m_language;
	}

	void Engine::Language(StormByte::Safe::String language) noexcept {
		if (m_language == language)
			return;
		m_language = std::move(language);
		if (m_model)
			m_model->m_open = false;
	}

	ExpectedText Engine::Recognize(
		std::span<const std::uint8_t> gray,
		int width,
		int height,
		int stride) noexcept {
		if (!m_model)
			return Unexpected<OCRException>("OCR engine is not available");
		constexpr int border = 10;
		if (width <= 0 || height <= 0 || stride < width
			|| width > std::numeric_limits<int>::max() - 2 * border
			|| height > std::numeric_limits<int>::max() - 2 * border
			|| static_cast<std::size_t>(height) > gray.size() / static_cast<std::size_t>(stride))
			return Unexpected<OCRException>("invalid OCR image");
		if (!m_model->Open(m_language)) {
			if (!m_language.empty())
				return std::unexpected<StormByte::Safe::Shared<StormByte::Multimedia::Exception>>(
					StormByte::Safe::Shared<StormByte::Multimedia::Exception>::MakePointer<TessDataNotFoundException>(
						static_cast<std::string_view>(m_language)));
			return std::unexpected<StormByte::Safe::Shared<StormByte::Multimedia::Exception>>(
				StormByte::Safe::Shared<StormByte::Multimedia::Exception>::MakePointer<TessDataNotFoundException>(
					FallbackLanguage));
		}

		const int paddedWidth = width + 2 * border;
		const int paddedHeight = height + 2 * border;
		if (static_cast<std::size_t>(paddedHeight) > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(paddedWidth))
			return Unexpected<OCRException>("OCR image is too large");
		std::vector<std::uint8_t> pixels;
		try {
			pixels.assign(static_cast<std::size_t>(paddedWidth) * static_cast<std::size_t>(paddedHeight), 255);
			for (int row = 0; row < height; ++row)
				std::copy_n(gray.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(stride), width,
					pixels.data() + static_cast<std::size_t>(row + border) * static_cast<std::size_t>(paddedWidth) + border);
		}
		catch (...) {
			return Unexpected<OCRException>("Could not allocate OCR image margins");
		}
		m_model->m_api.SetImage(pixels.data(), paddedWidth, paddedHeight, 1, paddedWidth);
		char* raw = m_model->m_api.GetUTF8Text();
		if (!raw)
			return Unexpected<OCRException>("tesseract returned no text");
		StormByte::Safe::String text(raw);
		delete[] raw;
		while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' '))
			text.pop_back();
		return text;
	}
}

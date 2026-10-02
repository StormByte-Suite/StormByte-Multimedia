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

#include <StormByte/multimedia/exception.hxx>
#include <StormByte/multimedia/visibility.h>

#include <string>
#include <string_view>

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
		 * @namespace StormByte::Multimedia::OCR
		 * @brief Private Tesseract OCR helpers.
		 */
		namespace OCR {
			/**
			 * @class TessDataNotFoundException
			 * @brief Thrown when the requested tessdata language blob is missing.
			 */
			class STORMBYTE_MULTIMEDIA_PRIVATE TessDataNotFoundException: public StormByte::Multimedia::Exception {
				public:
					/**
					 * @name Construction
					 * @{
					 */

					/**
					 * @brief Constructs the exception for @p lang.
					 * @param lang Requested tessdata key (`eng`, `spa`, …).
					 */
					explicit TessDataNotFoundException(std::string_view lang):
							StormByte::Multimedia::Exception(
								"OCR",
								"unsupported or missing language: {}",
								lang
							) {}

					/**
					 * @brief Copy constructor.
					 * @param other Exception to copy.
					 */
					TessDataNotFoundException(const TessDataNotFoundException& other);

					/**
					 * @brief Move constructor.
					 * @param other Exception to take.
					 */
					TessDataNotFoundException(TessDataNotFoundException&& other) noexcept;

					/**
					 * @brief Inherits the formatted Multimedia::Exception constructors.
					 */
					using StormByte::Multimedia::Exception::Exception;

					/**
					 * @brief Destructor. Defined in the Multimedia library to anchor RTTI.
					 */
					~TessDataNotFoundException() noexcept override;

					/**
					 * @brief Copy assignment.
					 * @param other Exception to copy.
					 * @return *this.
					 */
					TessDataNotFoundException& operator=(const TessDataNotFoundException& other);

					/**
					 * @brief Move assignment.
					 * @param other Exception to take.
					 * @return *this.
					 */
					TessDataNotFoundException& operator=(TessDataNotFoundException&& other) noexcept;

					/** @} */
			};

			/**
			 * @class OCRException
			 * @brief Thrown when the Tesseract session cannot open or recognize a cue.
			 */
			class STORMBYTE_MULTIMEDIA_PRIVATE OCRException: public StormByte::Multimedia::Exception {
				public:
					/**
					 * @name Construction
					 * @{
					 */

					/**
					 * @brief Constructs the exception with @p reason.
					 * @param reason Already worded failure text.
					 */
					explicit OCRException(std::string_view reason):
							StormByte::Multimedia::Exception(
								"OCR",
								"{}",
								reason
							) {}

					/**
					 * @brief Copy constructor.
					 * @param other Exception to copy.
					 */
					OCRException(const OCRException& other);

					/**
					 * @brief Move constructor.
					 * @param other Exception to take.
					 */
					OCRException(OCRException&& other) noexcept;

					/**
					 * @brief Inherits the formatted Multimedia::Exception constructors.
					 */
					using StormByte::Multimedia::Exception::Exception;

					/**
					 * @brief Destructor. Defined in the Multimedia library to anchor RTTI.
					 */
					~OCRException() noexcept override;

					/**
					 * @brief Copy assignment.
					 * @param other Exception to copy.
					 * @return *this.
					 */
					OCRException& operator=(const OCRException& other);

					/**
					 * @brief Move assignment.
					 * @param other Exception to take.
					 * @return *this.
					 */
					OCRException& operator=(OCRException&& other) noexcept;

					/** @} */
			};
		}
	}
}

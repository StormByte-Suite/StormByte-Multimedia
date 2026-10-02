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

#include <string>
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
		 * @namespace StormByte::Multimedia::FFmpeg
		 * @brief Private RAII wrappers over libav*.
		 */
		namespace FFmpeg {
			/**
			 * @class Exception
			 * @brief Base for FFmpeg backend errors.
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Exception: public Multimedia::Exception {
				public:
					/**
					 * @brief Constructs a formatted AV exception.
					 * @tparam Args Format argument types.
					 * @param component Subsystem label (`BSF`, `Decoder`, `Encoder`).
					 * @param fmt Format string.
					 * @param args Format arguments.
					 */
					template <typename... Args>
					Exception(std::string_view component, std::format_string<Args...> fmt, Args&&... args):
						Multimedia::Exception(
							std::string{"AV."}.append(component), fmt, std::forward<Args>(args)...) {}

					/**
					 * @brief Constructs from a preformatted message (`Unexpected<E>(fmt, …)`).
					 * @param message Already formatted text.
					 */
					explicit Exception(std::string_view message):
						Multimedia::Exception("AV", "{}", message) {}

					/**
					 * @brief Copy constructor.
					 * @param other Exception to copy.
					 */
					Exception(const Exception& other);

					/**
					 * @brief Move constructor.
					 * @param other Exception to take.
					 */
					Exception(Exception&& other) noexcept;

					/**
					 * @brief Destructor. Defined in the Multimedia library to anchor RTTI.
					 */
					~Exception() noexcept override;

					/**
					 * @brief Copy assignment.
					 * @param other Exception to copy.
					 * @return *this.
					 */
					Exception& operator=(const Exception& other);

					/**
					 * @brief Move assignment.
					 * @param other Exception to take.
					 * @return *this.
					 */
					Exception& operator=(Exception&& other) noexcept;
			};

			/**
			 * @class BSFError
			 * @brief Bitstream filter failure.
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC BSFError: public Exception {
				public:
					/**
					 * @brief Constructs a BSF error.
					 * @tparam Args Format argument types.
					 * @param fmt Format string.
					 * @param args Format arguments.
					 */
					template <typename... Args>
					BSFError(std::format_string<Args...> fmt, Args&&... args):
						Exception("BSF", fmt, std::forward<Args>(args)...) {}

					using Exception::Exception;

					/**
					 * @brief Copy constructor.
					 * @param other Exception to copy.
					 */
					BSFError(const BSFError& other);

					/**
					 * @brief Move constructor.
					 * @param other Exception to take.
					 */
					BSFError(BSFError&& other) noexcept;

					/**
					 * @brief Destructor. Defined in the Multimedia library to anchor RTTI.
					 */
					~BSFError() noexcept override;

					/**
					 * @brief Copy assignment.
					 * @param other Exception to copy.
					 * @return *this.
					 */
					BSFError& operator=(const BSFError& other);

					/**
					 * @brief Move assignment.
					 * @param other Exception to take.
					 * @return *this.
					 */
					BSFError& operator=(BSFError&& other) noexcept;
			};

			/**
			 * @class DecoderError
			 * @brief Decoder open or process failure.
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC DecoderError: public Exception {
				public:
					/**
					 * @brief Constructs a decoder error.
					 * @tparam Args Format argument types.
					 * @param fmt Format string.
					 * @param args Format arguments.
					 */
					template <typename... Args>
					DecoderError(std::format_string<Args...> fmt, Args&&... args):
						Exception("Decoder", fmt, std::forward<Args>(args)...) {}

					using Exception::Exception;

					/**
					 * @brief Copy constructor.
					 * @param other Exception to copy.
					 */
					DecoderError(const DecoderError& other);

					/**
					 * @brief Move constructor.
					 * @param other Exception to take.
					 */
					DecoderError(DecoderError&& other) noexcept;

					/**
					 * @brief Destructor. Defined in the Multimedia library to anchor RTTI.
					 */
					~DecoderError() noexcept override;

					/**
					 * @brief Copy assignment.
					 * @param other Exception to copy.
					 * @return *this.
					 */
					DecoderError& operator=(const DecoderError& other);

					/**
					 * @brief Move assignment.
					 * @param other Exception to take.
					 * @return *this.
					 */
					DecoderError& operator=(DecoderError&& other) noexcept;
			};

			/**
			 * @class EncoderError
			 * @brief Encoder open or process failure.
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC EncoderError: public Exception {
				public:
					/**
					 * @brief Constructs an encoder error.
					 * @tparam Args Format argument types.
					 * @param fmt Format string.
					 * @param args Format arguments.
					 */
					template <typename... Args>
					EncoderError(std::format_string<Args...> fmt, Args&&... args):
						Exception("Encoder", fmt, std::forward<Args>(args)...) {}

					using Exception::Exception;

					/**
					 * @brief Copy constructor.
					 * @param other Exception to copy.
					 */
					EncoderError(const EncoderError& other);

					/**
					 * @brief Move constructor.
					 * @param other Exception to take.
					 */
					EncoderError(EncoderError&& other) noexcept;

					/**
					 * @brief Destructor. Defined in the Multimedia library to anchor RTTI.
					 */
					~EncoderError() noexcept override;

					/**
					 * @brief Copy assignment.
					 * @param other Exception to copy.
					 * @return *this.
					 */
					EncoderError& operator=(const EncoderError& other);

					/**
					 * @brief Move assignment.
					 * @param other Exception to take.
					 * @return *this.
					 */
					EncoderError& operator=(EncoderError&& other) noexcept;
			};
		}
	}
}

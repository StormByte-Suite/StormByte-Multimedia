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

#include <StormByte/multimedia/exception.hxx>
#include <StormByte/safe/string.hxx>

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
			 * @brief Base for FFmpeg backend errors. `what()` is `StormByte.Multimedia.AV: message`.
			 *
			 * A further segment (`BSF`, `Decoder`, `Encoder`) is an @ref Path.
			 * Literal `{}` are replaced by @ref StormByte::Safe::String::ReplaceToken.
			 * This does not call `std::format`.
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Exception: public Multimedia::Exception {
				public:
					/**
					 * @struct Path
					 * @brief Additional path segments nested below `StormByte.Multimedia.AV`.
					 */
					struct Path {
						std::string_view segments; ///< Additional dotted path segments.

						/**
						 * @brief Wraps additional path segments.
						 * @param segments Dotted segments appended below `AV`. Empty adds none.
						 */
						explicit constexpr Path(std::string_view segments = {}) noexcept: segments(segments) {}
					};

					/**
					 * @brief Replace literal `{}` under `StormByte.Multimedia.AV`.
					 * @tparam Args Views, numbers, or types that convert to `std::string`. A number uses `std::to_string`.
					 * @param pattern Body with literal `{}` tokens. Not a `std::format` string.
					 * @param args Replacements. One per token, in order.
					 */
					template <typename... Args>
						requires (sizeof...(Args) >= 1)
					explicit Exception(std::string_view pattern, Args&&... args)
						: Exception(Path{}, pattern, std::forward<Args>(args)...) {}

					/**
					 * @brief Copy a borrowed plain message under `StormByte.Multimedia.AV`.
					 * @param message Exception text. Not a format string.
					 */
					explicit Exception(std::string_view message);

					/**
					 * @brief Replace literal `{}` under an additional AV path.
					 * @tparam Args Views, numbers, or types that convert to `std::string`. A number uses `std::to_string`.
					 * @param path Additional segments nested below `StormByte.Multimedia.AV`.
					 * @param pattern Body with literal `{}` tokens. Not a `std::format` string.
					 * @param args Replacements. One per token, in order.
					 */
					template <typename... Args>
						requires (sizeof...(Args) >= 1)
					explicit Exception(Path path, std::string_view pattern, Args&&... args)
						: Exception(path, static_cast<std::string_view>(StormByte::Safe::String::ReplaceToken(pattern, std::forward<Args>(args)...))) {}

					/**
					 * @brief Copy a borrowed plain message under an additional AV path.
					 * @param path Additional segments nested below `StormByte.Multimedia.AV`.
					 * @param message Exception text. Not a format string.
					 */
					explicit Exception(Path path, std::string_view message);

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
					 * @brief Replace literal `{}` under `StormByte.Multimedia.AV.BSF`.
					 * @tparam Args Views, numbers, or types that convert to `std::string`. A number uses `std::to_string`.
					 * @param pattern Body with literal `{}` tokens. Not a `std::format` string.
					 * @param args Replacements. One per token, in order.
					 */
					template <typename... Args>
						requires (sizeof...(Args) >= 1)
					BSFError(std::string_view pattern, Args&&... args):
						Exception(Path{"BSF"}, pattern, std::forward<Args>(args)...) {}

					/**
					 * @brief Copy a borrowed plain message under `StormByte.Multimedia.AV.BSF`.
					 * @param message Exception text. Not a format string.
					 */
					explicit BSFError(std::string_view message):
						Exception(Path{"BSF"}, message) {}

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
					 * @brief Replace literal `{}` under `StormByte.Multimedia.AV.Decoder`.
					 * @tparam Args Views, numbers, or types that convert to `std::string`. A number uses `std::to_string`.
					 * @param pattern Body with literal `{}` tokens. Not a `std::format` string.
					 * @param args Replacements. One per token, in order.
					 */
					template <typename... Args>
						requires (sizeof...(Args) >= 1)
					DecoderError(std::string_view pattern, Args&&... args):
						Exception(Path{"Decoder"}, pattern, std::forward<Args>(args)...) {}

					/**
					 * @brief Copy a borrowed plain message under `StormByte.Multimedia.AV.Decoder`.
					 * @param message Exception text. Not a format string.
					 */
					explicit DecoderError(std::string_view message):
						Exception(Path{"Decoder"}, message) {}

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
					 * @brief Replace literal `{}` under `StormByte.Multimedia.AV.Encoder`.
					 * @tparam Args Views, numbers, or types that convert to `std::string`. A number uses `std::to_string`.
					 * @param pattern Body with literal `{}` tokens. Not a `std::format` string.
					 * @param args Replacements. One per token, in order.
					 */
					template <typename... Args>
						requires (sizeof...(Args) >= 1)
					EncoderError(std::string_view pattern, Args&&... args):
						Exception(Path{"Encoder"}, pattern, std::forward<Args>(args)...) {}

					/**
					 * @brief Copy a borrowed plain message under `StormByte.Multimedia.AV.Encoder`.
					 * @param message Exception text. Not a format string.
					 */
					explicit EncoderError(std::string_view message):
						Exception(Path{"Encoder"}, message) {}

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

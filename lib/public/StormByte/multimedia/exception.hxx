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

#include <StormByte/exception.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/string.hxx>

#include <format>
#include <string_view>
#include <utility>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Public media types: codecs, containers, registry and stream kinds.
	 */
	namespace Multimedia {
		/**
		 * @class Exception
		 * @brief Base exception for the Multimedia module.
		 *
		 * The first argument is the subsystem tag (`File`, `Codec`). It is
		 * copied into a temporary @ref StormByte::Exception::Path; Base formats
		 * and copies the message during construction.
		 * @note Base automatically recognizes complete exception derivatives as
		 * MaybeSafe. This hierarchy keeps copy/move operations and destruction in
		 * the multimedia provider and messages in Base-owned storage. Compatible
		 * compiler, standard-library ABI and provider layout are required; Base and
		 * multimedia providers must remain loaded while exceptions are alive.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC Exception: public StormByte::Exception {
			public:
				/**
				 * @brief Constructs a formatted Multimedia exception.
				 * @tparam Args Format argument types.
				 * @param component Subsystem name (`File`, `Codec`, `Container`).
				 * @param fmt Format string.
				 * @param args Format arguments.
				 */
				template <typename... Args>
				Exception(std::string_view component, std::format_string<Args...> fmt, Args&&... args):
					StormByte::Exception(
						StormByte::Exception::Path{[component] {
							StormByte::Safe::String path{"Multimedia."};
							path += component;
							return path;
						}()},
						fmt, std::forward<Args>(args)...) {}

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
		 * @class CodecNotFoundException
		 * @brief Thrown when Registry::FindCodec does not resolve a key.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC CodecNotFoundException: public Exception {
			public:
				/**
				 * @brief Constructs the exception for @p codec.
				 * @param codec StormByte name or FFmpeg id that was not found.
				 */
				explicit CodecNotFoundException(std::string_view codec):
					Exception("Codec", "codec '{}' not found", codec) {}

				/**
				 * @brief Copy constructor.
				 * @param other Exception to copy.
				 */
				CodecNotFoundException(const CodecNotFoundException& other);

				/**
				 * @brief Move constructor.
				 * @param other Exception to take.
				 */
				CodecNotFoundException(CodecNotFoundException&& other) noexcept;

				/**
				 * @brief Destructor. Defined in the Multimedia library to anchor RTTI.
				 */
				~CodecNotFoundException() noexcept override;

				/**
				 * @brief Copy assignment.
				 * @param other Exception to copy.
				 * @return *this.
				 */
				CodecNotFoundException& operator=(const CodecNotFoundException& other);

				/**
				 * @brief Move assignment.
				 * @param other Exception to take.
				 * @return *this.
				 */
				CodecNotFoundException& operator=(CodecNotFoundException&& other) noexcept;
		};

		/**
		 * @class ContainerNotFoundException
		 * @brief Thrown when Registry::FindContainer does not resolve a key.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC ContainerNotFoundException: public Exception {
			public:
				/**
				 * @brief Constructs the exception for @p container.
				 * @param container StormByte name or FFmpeg format id that was not found.
				 */
				explicit ContainerNotFoundException(std::string_view container):
					Exception("Container", "container '{}' not found", container) {}

				/**
				 * @brief Copy constructor.
				 * @param other Exception to copy.
				 */
				ContainerNotFoundException(const ContainerNotFoundException& other);

				/**
				 * @brief Move constructor.
				 * @param other Exception to take.
				 */
				ContainerNotFoundException(ContainerNotFoundException&& other) noexcept;

				/**
				 * @brief Destructor. Defined in the Multimedia library to anchor RTTI.
				 */
				~ContainerNotFoundException() noexcept override;

				/**
				 * @brief Copy assignment.
				 * @param other Exception to copy.
				 * @return *this.
				 */
				ContainerNotFoundException& operator=(const ContainerNotFoundException& other);

				/**
				 * @brief Move assignment.
				 * @param other Exception to take.
				 * @return *this.
				 */
				ContainerNotFoundException& operator=(ContainerNotFoundException&& other) noexcept;
		};

		/**
		 * @class FileOpenException
		 * @brief Thrown when File::Open fails (path or buffer).
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC FileOpenException: public Exception {
			public:
				/**
				 * @brief Constructs the exception with a finished message.
				 * @param message Already formatted reason text.
				 */
				explicit FileOpenException(std::string_view message):
					Exception("File", "{}", message) {}

				/**
				 * @brief Copy constructor.
				 * @param other Exception to copy.
				 */
				FileOpenException(const FileOpenException& other);

				/**
				 * @brief Move constructor.
				 * @param other Exception to take.
				 */
				FileOpenException(FileOpenException&& other) noexcept;

				/**
				 * @brief Destructor. Defined in the Multimedia library to anchor RTTI.
				 */
				~FileOpenException() noexcept override;

				/**
				 * @brief Copy assignment.
				 * @param other Exception to copy.
				 * @return *this.
				 */
				FileOpenException& operator=(const FileOpenException& other);

				/**
				 * @brief Move assignment.
				 * @param other Exception to take.
				 * @return *this.
				 */
				FileOpenException& operator=(FileOpenException&& other) noexcept;
		};

		/**
		 * @class FilePathOpenException
		 * @brief Thrown when File::Open fails on a filesystem path.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC FilePathOpenException: public FileOpenException {
			public:
				/**
				 * @brief Constructs the exception for @p path.
				 * @param path Filesystem path.
				 * @param reason Why Open failed.
				 */
				explicit FilePathOpenException(std::string_view path, std::string_view reason):
					FileOpenException(std::format("failed to open '{}': {}", path, reason)) {}

				/**
				 * @brief Copy constructor.
				 * @param other Exception to copy.
				 */
				FilePathOpenException(const FilePathOpenException& other);

				/**
				 * @brief Move constructor.
				 * @param other Exception to take.
				 */
				FilePathOpenException(FilePathOpenException&& other) noexcept;

				/**
				 * @brief Destructor. Defined in the Multimedia library to anchor RTTI.
				 */
				~FilePathOpenException() noexcept override;

				/**
				 * @brief Copy assignment.
				 * @param other Exception to copy.
				 * @return *this.
				 */
				FilePathOpenException& operator=(const FilePathOpenException& other);

				/**
				 * @brief Move assignment.
				 * @param other Exception to take.
				 * @return *this.
				 */
				FilePathOpenException& operator=(FilePathOpenException&& other) noexcept;
		};

		/**
		 * @class FileBufferOpenException
		 * @brief Thrown when File::Open fails on a Consumer.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC FileBufferOpenException: public FileOpenException {
			public:
				/**
				 * @brief Constructs the exception.
				 * @param reason Why Open failed (empty, corrupt, I/O).
				 */
				explicit FileBufferOpenException(std::string_view reason):
					FileOpenException(std::format("failed to open buffer: {}", reason)) {}

				/**
				 * @brief Copy constructor.
				 * @param other Exception to copy.
				 */
				FileBufferOpenException(const FileBufferOpenException& other);

				/**
				 * @brief Move constructor.
				 * @param other Exception to take.
				 */
				FileBufferOpenException(FileBufferOpenException&& other) noexcept;

				/**
				 * @brief Destructor. Defined in the Multimedia library to anchor RTTI.
				 */
				~FileBufferOpenException() noexcept override;

				/**
				 * @brief Copy assignment.
				 * @param other Exception to copy.
				 * @return *this.
				 */
				FileBufferOpenException& operator=(const FileBufferOpenException& other);

				/**
				 * @brief Move assignment.
				 * @param other Exception to take.
				 * @return *this.
				 */
				FileBufferOpenException& operator=(FileBufferOpenException&& other) noexcept;
		};

		/**
		 * @class TranscodeException
		 * @brief Thrown when Transcode::Open or a configuration call fails.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC TranscodeException: public Exception {
			public:
				/**
				 * @brief Constructs the exception with a finished message.
				 * @param message Already formatted reason text.
				 */
				explicit TranscodeException(std::string_view message):
					Exception("Transcode", "{}", message) {}

				/**
				 * @brief Copy constructor.
				 * @param other Exception to copy.
				 */
				TranscodeException(const TranscodeException& other);

				/**
				 * @brief Move constructor.
				 * @param other Exception to take.
				 */
				TranscodeException(TranscodeException&& other) noexcept;

				/**
				 * @brief Destructor. Defined in the Multimedia library to anchor RTTI.
				 */
				~TranscodeException() noexcept override;

				/**
				 * @brief Copy assignment.
				 * @param other Exception to copy.
				 * @return *this.
				 */
				TranscodeException& operator=(const TranscodeException& other);

				/**
				 * @brief Move assignment.
				 * @param other Exception to take.
				 * @return *this.
				 */
				TranscodeException& operator=(TranscodeException&& other) noexcept;
		};
	}
}

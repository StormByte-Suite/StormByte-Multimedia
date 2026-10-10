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
#include <StormByte/multimedia/exception.hxx>
#include <StormByte/multimedia/stream.hxx>
#include <StormByte/safe/vector.hxx>

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
		 * @class Codec
		 * @brief Codec identity owned by the multimedia registry.
		 */
		class Codec;
		/**
		 * @class Container
		 * @brief Container identity owned by the multimedia registry.
		 */
		class Container;
		/**
		 * @class File
		 * @brief Multimedia file facade.
		 */
		class File;
		/**
		 * @class Stream
		 * @brief Multimedia stream facade.
		 */
		class Stream;

		/**
		 * @class CodecRef
		 * @brief Trivial borrowed codec handle; never owns or deletes the codec.
		 * @note The codec must outlive every handle. Registry handles are valid until
		 * registry teardown, with the provider loaded and a compatible compiler,
		 * standard-library ABI and provider layout. Empty handles cannot be dereferenced.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC CodecRef {
			public:
				/**
				 * @brief Constructs an empty handle.
				 */
				constexpr CodecRef() noexcept = default;

				/**
				 * @brief Borrows a codec.
				 * @param codec Codec that must outlive the handle.
				 */
				constexpr CodecRef(const Codec& codec) noexcept: m_codec(&codec) {}

				/**
				 * @brief Copies the borrowed pointer.
				 * @param other Source handle.
				 */
				constexpr CodecRef(const CodecRef& other) noexcept = default;

				/**
				 * @brief Copies the borrowed pointer without clearing the source.
				 * @param other Source handle.
				 */
				constexpr CodecRef(CodecRef&& other) noexcept = default;

				/**
				 * @brief Destroys the handle, not its codec.
				 */
				~CodecRef() noexcept = default;

				/**
				 * @brief Copies the borrowed pointer.
				 * @param other Source handle.
				 * @return This handle.
				 */
				constexpr CodecRef& operator=(const CodecRef& other) noexcept = default;

				/**
				 * @brief Copies the borrowed pointer without clearing the source.
				 * @param other Source handle.
				 * @return This handle.
				 */
				constexpr CodecRef& operator=(CodecRef&& other) noexcept = default;

				/**
				 * @brief Tests whether a codec is borrowed.
				 * @return True for a nonempty handle.
				 */
				constexpr explicit operator bool() const noexcept { return m_codec != nullptr; }

				/**
				 * @brief Gets the borrowed codec, preserving reference_wrapper syntax.
				 * @pre The handle is nonempty and its codec is alive.
				 * @return Borrowed codec.
				 */
				constexpr const Codec& get() const noexcept { return *m_codec; }

				/**
				 * @brief Converts to the borrowed codec reference.
				 * @pre The handle is nonempty and its codec is alive.
				 * @return Borrowed codec.
				 */
				constexpr operator const Codec&() const noexcept { return get(); }

			private:
				const Codec* m_codec = nullptr;	///< Borrowed codec; never deleted.
		};

		/**
		 * @class ContainerRef
		 * @brief Trivial borrowed container handle; never owns or deletes the container.
		 * @note The container must outlive every handle. Registry handles are valid until
		 * registry teardown, with the provider loaded and a compatible compiler,
		 * standard-library ABI and provider layout. Empty handles cannot be dereferenced.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC ContainerRef {
			public:
				/**
				 * @brief Constructs an empty handle.
				 */
				constexpr ContainerRef() noexcept = default;

				/**
				 * @brief Borrows a container.
				 * @param container Container that must outlive the handle.
				 */
				constexpr ContainerRef(const Container& container) noexcept: m_container(&container) {}

				/**
				 * @brief Copies the borrowed pointer.
				 * @param other Source handle.
				 */
				constexpr ContainerRef(const ContainerRef& other) noexcept = default;

				/**
				 * @brief Copies the borrowed pointer without clearing the source.
				 * @param other Source handle.
				 */
				constexpr ContainerRef(ContainerRef&& other) noexcept = default;

				/**
				 * @brief Destroys the handle, not its container.
				 */
				~ContainerRef() noexcept = default;

				/**
				 * @brief Copies the borrowed pointer.
				 * @param other Source handle.
				 * @return This handle.
				 */
				constexpr ContainerRef& operator=(const ContainerRef& other) noexcept = default;

				/**
				 * @brief Copies the borrowed pointer without clearing the source.
				 * @param other Source handle.
				 * @return This handle.
				 */
				constexpr ContainerRef& operator=(ContainerRef&& other) noexcept = default;

				/**
				 * @brief Tests whether a container is borrowed.
				 * @return True for a nonempty handle.
				 */
				constexpr explicit operator bool() const noexcept { return m_container != nullptr; }

				/**
				 * @brief Gets the borrowed container, preserving reference_wrapper syntax.
				 * @pre The handle is nonempty and its container is alive.
				 * @return Borrowed container.
				 */
				constexpr const Container& get() const noexcept { return *m_container; }

				/**
				 * @brief Converts to the borrowed container reference.
				 * @pre The handle is nonempty and its container is alive.
				 * @return Borrowed container.
				 */
				constexpr operator const Container&() const noexcept { return get(); }

			private:
				const Container* m_container = nullptr;	///< Borrowed container; never deleted.
		};
	}
}

/**
 * @brief Declare the borrowed codec handle conditionally DLL-safe.
 * @note The codec and compatible multimedia provider must remain alive during use.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::CodecRef);

/**
 * @brief Declare the borrowed container handle conditionally DLL-safe.
 * @note The container and compatible multimedia provider must remain alive during use.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::ContainerRef);

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
		 * @namespace StormByte::Multimedia::Pipeline
		 * @brief Demux / decode / filter / encode / mux types.
		 *
		 * @ingroup multimedia_pipeline
		 */
		namespace Pipeline {
			/**
			 * @class Transcoder
			 * @brief File-to-file job facade.
			 */
			class Transcoder;
		}

		/**
		 * @typedef ExpectedCodec
		 * @brief Result of FindCodec.
		 * @note Borrows a registry identity; compatible ABI and provider lifetime
		 * are required. This alias does not acquire codec ownership.
		 */
		using ExpectedCodec = StormByte::Expected<CodecRef, CodecNotFoundException>;

		/**
		 * @typedef ExpectedContainer
		 * @brief Result of FindContainer.
		 * @note Borrows a registry identity; compatible ABI and provider lifetime
		 * are required. This alias does not acquire container ownership.
		 */
		using ExpectedContainer = StormByte::Expected<ContainerRef, ContainerNotFoundException>;

		/**
		 * @typedef ExpectedFile
		 * @brief Result of OpenFile.
		 */
		using ExpectedFile = StormByte::Expected<File, FileOpenException>;

		/**
		 * @typedef ExpectedTranscoder
		 * @brief Result of Transcoder::Open.
		 */
		using ExpectedTranscoder = StormByte::Expected<Safe::Unique<Pipeline::Transcoder>, TranscodeException>;

		/**
		 * @typedef CodecRefs
		 * @brief Safe-owned list of borrowed codecs, not codec ownership.
		 * @note Each nonempty handle requires its codec and provider to remain alive.
		 * Safe owner callbacks retain allocation and destruction in the owning module.
		 */
		using CodecRefs = Safe::Vector<CodecRef>;

		/**
		 * @typedef ContainerRefs
		 * @brief Safe-owned list of borrowed containers, not container ownership.
		 * @note Each nonempty handle requires its container and provider to remain alive.
		 * Safe owner callbacks retain allocation and destruction in the owning module.
		 */
		using ContainerRefs = Safe::Vector<ContainerRef>;
	}
}

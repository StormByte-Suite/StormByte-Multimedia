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

#include <StormByte/buffer/fifo.hxx>
#include <StormByte/multimedia/visibility.h>

#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/vector.hxx>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Multimedia module of the StormByte suite.
	 */
	namespace Multimedia {
		/**
		 * @class Attachment
		 * @brief Container attachment (cover, fonts). Not a Stream.
		 *
		 * avformat exposes Matroska attached files as fake video tracks
		 * (`AV_DISPOSITION_ATTACHED_PIC`). This type is the real contract.
		 * Metadata and payload retain creator-owned storage. Base, Buffer and
		 * Multimedia providers must remain loaded while attachments are in use.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC Attachment {
			public:
				/**
				 * @brief Constructs an empty attachment.
				 * @throws StormByte::Exception Safe metadata initialization failed.
				 */
				Attachment();

				/**
				 * @brief Builds an attachment.
				 * @param fileName Source file name, if known.
				 * @param mimeType MIME type, if known.
				 * @param payload File bytes.
				 */
				Attachment(StormByte::Safe::Optional<StormByte::Safe::String> fileName,
					StormByte::Safe::Optional<StormByte::Safe::String> mimeType,
					StormByte::Buffer::FIFO payload) noexcept;

				/**
				 * @brief Copies attachment metadata and payload state.
				 * @param other Attachment to copy.
				 * @throws StormByte::Exception Safe metadata copying failed.
				 */
				Attachment(const Attachment& other);

				/**
				 * @brief Move constructor.
				 * @param other Attachment to take.
				 */
				Attachment(Attachment&& other) noexcept;

				/**
				 * @brief Destructor.
				 */
				~Attachment() noexcept;

				/**
				 * @brief Copies attachment metadata and payload state.
				 * @param other Attachment to copy.
				 * @return *this.
				 * @throws StormByte::Exception Safe storage copying failed.
				 */
				Attachment& operator=(const Attachment& other);

				/**
				 * @brief Move assignment.
				 * @param other Attachment to take.
				 * @return *this.
				 */
				Attachment& operator=(Attachment&& other) noexcept;

				/**
				 * @brief Original file name.
				 * @return Borrowed Safe optional, empty when the name is unknown.
				 */
				const StormByte::Safe::Optional<StormByte::Safe::String>& FileName() const noexcept;

				/**
				 * @brief MIME type.
				 * @return Borrowed Safe optional, empty when the type is unknown.
				 */
				const StormByte::Safe::Optional<StormByte::Safe::String>& MimeType() const noexcept;

				/**
				 * @brief Attachment bytes.
				 * @return Borrowed FIFO, valid while this attachment is alive.
				 */
				const StormByte::Buffer::FIFO& Payload() const noexcept;

			private:
				StormByte::Safe::Optional<StormByte::Safe::String> m_fileName;	///< File name
				StormByte::Safe::Optional<StormByte::Safe::String> m_mimeType;	///< MIME type
				StormByte::Buffer::FIFO m_payload;								///< Bytes
		};
	}
}

/**
 * @brief Registers attachments with provider-local move and destruction operations.
 * @note Attachment metadata and payload retain creator-owned Safe storage.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Attachment);

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Multimedia module of the StormByte suite.
	 */
	namespace Multimedia {
		/**
		 * @brief Ordered attachments with creator-owned Safe storage.
		 */
		using Attachments = StormByte::Safe::Vector<Attachment>;
	}
}

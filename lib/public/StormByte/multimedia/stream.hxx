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

#include <StormByte/multimedia/codec.hxx>
#include <StormByte/multimedia/metadata/stream.hxx>
#include <StormByte/multimedia/property/audio.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/property/video.hxx>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pair.hxx>
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
		 * @brief Public media file snapshot.
		 */
		class File;

		/**
		 * @class Stream
		 * @brief One media stream: registry Codec, duration, properties and tags.
		 *
		 * Copies share the same Codec instance. The Codec outlives every Stream.
		 * Default construction creates an empty stream with index -1 and Unknown type.
		 * Codec() throws for an empty stream. Copies retain a borrowed registry codec.
		 * Base, Multimedia and the registry must outlive all snapshots and callbacks.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC Stream {
			public:
				/**
				 * @brief Safe video and audio property snapshots, respectively.
				 * @note At most one optional is populated for a probed stream.
				 */
				using Properties = StormByte::Safe::Pair<
					StormByte::Safe::Optional<Property::Video>,
					StormByte::Safe::Optional<Property::Audio>>;

				/**
				 * @brief Constructs an empty stream snapshot.
				 * @throws StormByte::Exception Safe storage initialization failed.
				 */
				Stream();

				/**
				 * @brief Copy constructor.
				 * @param other Snapshot to copy.
				 * @throws StormByte::Exception Safe storage copying failed.
				 */
				Stream(const Stream& other);

				/**
				 * @brief Move constructor.
				 * @param other Snapshot to take.
				 */
				Stream(Stream&& other) noexcept;

				/**
				 * @brief Destructor.
				 */
				~Stream() noexcept;

				/**
				 * @brief Copies a snapshot and its borrowed codec.
				 * @param other Snapshot to copy.
				 * @return *this.
				 * @throws StormByte::Exception Safe storage copying failed.
				 */
				Stream& operator=(const Stream& other);

				/**
				 * @brief Transfers a snapshot and its borrowed codec.
				 * @param other Snapshot to take.
				 * @return *this.
				 */
				Stream& operator=(Stream&& other) noexcept;

				/**
				 * @brief Container stream index (avformat). Not the position in File::Streams().
				 * @return Index used by packets and Decoder.
				 */
				int Index() const noexcept { return m_index; }

				/**
				 * @brief Codec of this stream.
				 * @return Registry codec.
				 * @throws StormByte::Multimedia::Exception This snapshot is empty.
				 */
				const class Codec& Codec() const;

				/**
				 * @brief Media kind of the codec.
				 * @return Audio, Video, Subtitle, Attachment or Unknown.
				 */
				Type Type() const noexcept;

				/**
				 * @brief Per-stream tags captured at Open.
				 * @return Metadata snapshot.
				 */
				const Metadata::Stream& Metadata() const noexcept { return m_metadata; }

				/**
				 * @brief Stream duration.
				 * @return Duration, or empty if unknown.
				 *
				 * Header value from Open, or a value filled by File::Duration() after
				 * a packet scan. Empty if it cannot be determined.
				 */
				const StormByte::Safe::Optional<Property::Duration>& Duration() const noexcept;

				/**
				 * @brief Video properties when this stream is video.
				 * @return Borrowed Safe optional, empty when video properties are unavailable.
				 * @note Arrow access yields a temporary read-only snapshot, not retained storage.
				 */
				const StormByte::Safe::Optional<Property::Video>& Video() const noexcept;

				/**
				 * @brief Audio properties when this stream is audio.
				 * @return Borrowed Safe optional, empty when audio properties are unavailable.
				 * @note Arrow access yields a temporary read-only snapshot, not retained storage.
				 */
				const StormByte::Safe::Optional<Property::Audio>& Audio() const noexcept;

			private:
				friend class File;

				int m_index;												///< avformat stream index, or -1 when empty
				const class Codec* m_codec;									///< Borrowed registry codec, or nullptr when empty
				Metadata::Stream m_metadata;									///< Stream tags
				mutable StormByte::Safe::Optional<Property::Duration> m_duration;	///< Stream duration
				Properties m_properties;										///< Video, audio, or none

				/**
				 * @brief File-only constructor.
				 * @param index avformat stream index.
				 * @param codec Registry codec.
				 * @param metadata Stream tags.
				 * @param duration Stream duration from the header, if any.
				 * @param properties Typed property bag.
				 */
				Stream(int index, const class Codec& codec, Metadata::Stream metadata,
					StormByte::Safe::Optional<Property::Duration> duration, Properties properties) noexcept;
		};
	}
}

/**
 * @brief Registers completed Stream snapshots with exported value operations.
 * @note Registry codecs are synchronous borrows; providers must remain loaded.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Stream);

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
		 * @brief Ordered stream snapshots with creator-owned Safe storage.
		 */
		using Streams = StormByte::Safe::Vector<Stream>;
	}
}

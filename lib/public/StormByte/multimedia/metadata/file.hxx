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

#include <StormByte/multimedia/visibility.h>

#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/string.hxx>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Multimedia processing and metadata.
	 */
	namespace Multimedia {
	/**
	 * @class File
	 * @brief Public media file snapshot.
	 */
	class File;

	/**
	 * @namespace StormByte::Multimedia::Detail
	 * @brief Private multimedia implementation helpers.
	 */
	namespace Detail {
	/**
	 * @class Probe
	 * @brief Private metadata probe.
	 */
	class Probe;
	}

/**
 * @namespace StormByte::Multimedia::Metadata
 * @brief Snapshot metadata for files and streams.
 */
	namespace Metadata {
	/**
	 * @class File
	 * @brief Container-level tags and header fields captured at Open.
	 *
	 * Duration that requires a demux scan lives on Multimedia::File, not here.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC File {
		public:
			/**
			 * @brief Copy constructor.
			 * @param other Metadata to copy.
			 */
			File(const File& other);

			/**
			 * @brief Move constructor.
			 * @param other Metadata to transfer.
			 */
			File(File&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			~File() noexcept;

			/**
			 * @brief Copy assignment.
			 * @param other Metadata to copy.
			 * @return *this.
			 */
			File& operator=(const File& other);

			/**
			 * @brief Move assignment.
			 * @param other Metadata to transfer.
			 * @return *this.
			 */
			File& operator=(File&& other) noexcept;

			/**
			 * @brief Work title.
			 * @return Title, or empty.
			 */
			const Safe::Optional<Safe::String>& Title() const noexcept;

			/**
			 * @brief Main artist.
			 * @return Artist, or empty.
			 */
			const Safe::Optional<Safe::String>& Artist() const noexcept;

			/**
			 * @brief Album / set name.
			 * @return Album, or empty.
			 */
			const Safe::Optional<Safe::String>& Album() const noexcept;

			/**
			 * @brief Album artist if different from Artist.
			 * @return Album artist, or empty.
			 */
			const Safe::Optional<Safe::String>& AlbumArtist() const noexcept;

			/**
			 * @brief Composer.
			 * @return Composer, or empty.
			 */
			const Safe::Optional<Safe::String>& Composer() const noexcept;

			/**
			 * @brief Genre.
			 * @return Genre, or empty.
			 */
			const Safe::Optional<Safe::String>& Genre() const noexcept;

			/**
			 * @brief Comment.
			 * @return Comment, or empty.
			 */
			const Safe::Optional<Safe::String>& Comment() const noexcept;

			/**
			 * @brief Copyright notice.
			 * @return Copyright, or empty.
			 */
			const Safe::Optional<Safe::String>& Copyright() const noexcept;

			/**
			 * @brief Encoder identification.
			 * @return Encoder, or empty.
			 */
			const Safe::Optional<Safe::String>& Encoder() const noexcept;

			/**
			 * @brief Date / year tag.
			 * @return Date, or empty.
			 */
			const Safe::Optional<Safe::String>& Date() const noexcept;

			/**
			 * @brief Track number.
			 * @return Track, or empty.
			 */
			Safe::Optional<unsigned> Track() const;

			/**
			 * @brief Disc number.
			 * @return Disc, or empty.
			 */
			Safe::Optional<unsigned> Disc() const;

		private:
			friend class StormByte::Multimedia::File;
			friend class StormByte::Multimedia::Detail::Probe;

			Safe::Optional<Safe::String> m_title;		///< Work title.

			Safe::Optional<Safe::String> m_artist;		///< Main artist.

			Safe::Optional<Safe::String> m_album;		///< Album or set name.

			Safe::Optional<Safe::String> m_albumArtist;	///< Album artist.

			Safe::Optional<Safe::String> m_composer;	///< Composer.

			Safe::Optional<Safe::String> m_genre;		///< Genre.

			Safe::Optional<Safe::String> m_comment;		///< Comment.

			Safe::Optional<Safe::String> m_copyright;	///< Copyright notice.

			Safe::Optional<Safe::String> m_encoder;		///< Encoder identification.

			Safe::Optional<Safe::String> m_date;		///< Date or year tag.

			Safe::Optional<unsigned> m_track;			///< Track number.

			Safe::Optional<unsigned> m_disc;			///< Disc number.

			/**
			 * @brief Empty metadata.
			 */
			File();

			/**
			 * @brief Sets the title.
			 * @param title Title tag.
			 */
			void Title(Safe::String title);

			/**
			 * @brief Sets the artist.
			 * @param artist Artist tag.
			 */
			void Artist(Safe::String artist);

			/**
			 * @brief Sets the album.
			 * @param album Album tag.
			 */
			void Album(Safe::String album);

			/**
			 * @brief Sets the album artist.
			 * @param albumArtist Album artist tag.
			 */
			void AlbumArtist(Safe::String albumArtist);

			/**
			 * @brief Sets the composer.
			 * @param composer Composer tag.
			 */
			void Composer(Safe::String composer);

			/**
			 * @brief Sets the genre.
			 * @param genre Genre tag.
			 */
			void Genre(Safe::String genre);

			/**
			 * @brief Sets the comment.
			 * @param comment Comment tag.
			 */
			void Comment(Safe::String comment);

			/**
			 * @brief Sets the copyright.
			 * @param copyright Copyright tag.
			 */
			void Copyright(Safe::String copyright);

			/**
			 * @brief Sets the encoder.
			 * @param encoder Encoder tag.
			 */
			void Encoder(Safe::String encoder);

			/**
			 * @brief Sets the date.
			 * @param date Date tag.
			 */
			void Date(Safe::String date);

			/**
			 * @brief Sets the track number.
			 * @param track Track number.
			 */
			void Track(unsigned track);

			/**
			 * @brief Sets the disc number.
			 * @param disc Disc number.
			 */
			void Disc(unsigned disc);
	};
	}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Metadata::File);

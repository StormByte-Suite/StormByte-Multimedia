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
		 * @namespace StormByte::Multimedia::Metadata
		 * @brief Snapshot metadata for files and streams.
		 */
		namespace Metadata {
			/**
			 * @enum FileTag
			 * @brief File-level tag that a job can keep, replace or remove.
			 *
			 * MP3 stores these as ID3, Ogg and FLAC as Vorbis comments, MP4 as
			 * iTunes atoms and Matroska as global tags.
			 */
			enum class FileTag: unsigned char {
				Title,			///< Work title.
				Artist,			///< Main artist.
				Album,			///< Album or set name.
				AlbumArtist,	///< Album artist.
				Composer,		///< Composer.
				Genre,			///< Genre.
				Comment,		///< Comment.
				Copyright,		///< Copyright notice.
				Date,			///< Date or year.
				Track,			///< Track number.
				Disc			///< Disc number.
			};

			/**
			 * @brief Number of @ref StormByte::Multimedia::Metadata::FileTag values.
			 */
			inline constexpr unsigned FileTagCount = 11;

			/**
			 * @enum StreamTag
			 * @brief Per-track tag that a job can keep, replace or remove.
			 */
			enum class StreamTag: unsigned char {
				Language,		///< ISO language of the track.
				Title			///< Track title.
			};

			/**
			 * @brief Number of @ref StormByte::Multimedia::Metadata::StreamTag values.
			 */
			inline constexpr unsigned StreamTagCount = 2;
		}
	}
}

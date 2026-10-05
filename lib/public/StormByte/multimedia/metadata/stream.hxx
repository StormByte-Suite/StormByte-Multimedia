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

#include <StormByte/bitmask.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/string.hxx>

#include <cstdint>

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
	 * @class Stream
	 * @brief Public media stream snapshot.
	 */
	class Stream;

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
	 * @class Stream
	 * @brief Per-stream tags and header fields captured at Open.
	 */
	class Stream;

	/**
	 * @enum DispositionFlag
	 * @brief Per-stream disposition bits (FFmpeg AV_DISPOSITION_* subset).
	 */
	enum class STORMBYTE_MULTIMEDIA_PUBLIC DispositionFlag: std::uint16_t {
		/**
		 * @brief No flags.
		 */
		None = 0,

		/**
		 * @brief Default playback stream.
		 */
		Default = 1 << 0,

		/**
		 * @brief Dubbed audio.
		 */
		Dub = 1 << 1,

		/**
		 * @brief Original language.
		 */
		Original = 1 << 2,

		/**
		 * @brief Commentary.
		 */
		Comment = 1 << 3,

		/**
		 * @brief Lyrics.
		 */
		Lyrics = 1 << 4,

		/**
		 * @brief Karaoke.
		 */
		Karaoke = 1 << 5,

		/**
		 * @brief Forced subtitles.
		 */
		Forced = 1 << 6,

		/**
		 * @brief Hearing-impaired content.
		 */
		HearingImpaired = 1 << 7,

		/**
		 * @brief Visually impaired content.
		 */
		VisualImpaired = 1 << 8,

		/**
		 * @brief Cover or attached picture.
		 */
		AttachedPicture = 1 << 9
	};

	/**
	 * @class Disposition
	 * @brief Bitmask of DispositionFlag.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Disposition: public StormByte::Bitmask<Disposition, DispositionFlag> {
		public:
			/**
			 * @brief Copy constructor.
			 * @param disposition Source mask.
			 */
			Disposition(const Disposition& disposition) noexcept;

			/**
			 * @brief Move constructor.
			 * @param disposition Source mask.
			 */
			Disposition(Disposition&& disposition) noexcept;

			/**
			 * @brief Destructor.
			 */
			~Disposition() noexcept override;

			/**
			 * @brief Copy assignment.
			 * @param disposition Source mask.
			 * @return *this.
			 */
			Disposition& operator=(const Disposition& disposition) noexcept;

			/**
			 * @brief Move assignment.
			 * @param disposition Source mask.
			 * @return *this.
			 */
			Disposition& operator=(Disposition&& disposition) noexcept;

		private:
			friend class Stream;
			friend class StormByte::Multimedia::File;
			friend class StormByte::Multimedia::Detail::Probe;
			friend class StormByte::Bitmask<Disposition, DispositionFlag>;

			/**
			 * @brief Empty mask.
			 */
			Disposition() noexcept;

			/**
			 * @brief Mask from a single flag.
			 * @param flag Initial flag.
			 */
			Disposition(DispositionFlag flag) noexcept;
	};
	}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Metadata::Disposition);

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
	 * @class Stream
	 * @brief Per-stream tags and header fields captured at Open.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Stream {
		public:
			/**
			 * @brief Copy constructor.
			 * @param other Metadata to copy.
			 */
			Stream(const Stream& other);

			/**
			 * @brief Move constructor.
			 * @param other Metadata to transfer.
			 */
			Stream(Stream&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			~Stream() noexcept;

			/**
			 * @brief Copy assignment.
			 * @param other Metadata to copy.
			 * @return *this.
			 */
			Stream& operator=(const Stream& other);

			/**
			 * @brief Move assignment.
			 * @param other Metadata to transfer.
			 * @return *this.
			 */
			Stream& operator=(Stream&& other) noexcept;

			/**
			 * @brief Stream title.
			 * @return Title, or empty.
			 */
			const Safe::Optional<Safe::String>& Title() const noexcept;

			/**
			 * @brief Language code (usually ISO 639).
			 * @return Language, or empty.
			 */
			const Safe::Optional<Safe::String>& Language() const noexcept;

			/**
			 * @brief Stream bitrate in bits per second.
			 * @return Bitrate, or empty.
			 */
			Safe::Optional<std::uint64_t> BitRate() const;

			/**
			 * @brief Disposition flags.
			 * @return Mask (may be empty).
			 */
			class Disposition Disposition() const noexcept;

		private:
			friend class StormByte::Multimedia::File;
			friend class StormByte::Multimedia::Stream;
			friend class StormByte::Multimedia::Detail::Probe;

			Safe::Optional<Safe::String> m_title;		///< Stream title.

			Safe::Optional<Safe::String> m_language;	///< Language code.

			Safe::Optional<std::uint64_t> m_bitRate;	///< Bitrate in bits per second.

			class Disposition m_disposition;			///< Disposition flags.

			/**
			 * @brief Empty metadata.
			 */
			Stream();

			/**
			 * @brief Sets the stream title.
			 * @param title Title tag.
			 */
			void Title(Safe::String title);

			/**
			 * @brief Sets the language.
			 * @param language Language tag.
			 */
			void Language(Safe::String language);

			/**
			 * @brief Sets the stream bitrate.
			 * @param bitRate Bits per second.
			 */
			void BitRate(std::uint64_t bitRate);

			/**
			 * @brief Sets the disposition mask.
			 * @param disposition Flags.
			 */
			void Disposition(class Disposition disposition) noexcept;
	};
		}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Metadata::Stream);

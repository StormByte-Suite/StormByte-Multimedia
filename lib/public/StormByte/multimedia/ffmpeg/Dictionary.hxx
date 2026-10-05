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

#include <StormByte/multimedia/ffmpeg/AVPointer.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/visibility.h>

#include <string>

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	/**
	 * @class Dictionary
	 * @brief RAII `AVDictionary` for muxer metadata / options.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Dictionary: public AVPointer<::AVDictionary> {
		friend class AVFormatContext;
		public:
			/**
			 * @brief Empty dictionary.
			 */
			Dictionary() noexcept;

			/**
			 * @brief Move constructor. Transfers the dictionary.
			 * @param other Source dictionary; left empty.
			 */
			Dictionary(Dictionary&& other) noexcept = default;

			/**
			 * @brief Destructor. Frees the `AVDictionary`.
			 */
			~Dictionary() noexcept override;

			/**
			 * @brief Move assignment. Frees *this, then takes @p other.
			 * @param other Source dictionary; left empty.
			 * @return *this.
			 */
			Dictionary& operator=(Dictionary&& other) noexcept = default;

			/**
			 * @brief Whether any entry is stored.
			 * @return true if the dictionary is non-empty.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief Sets @p key to @p value (`av_dict_set`).
			 * @param key Entry key.
			 * @param value Entry value. nullptr deletes the key.
			 * @param flags `AV_DICT_*` flags.
			 * @return false on failure.
			 */
			bool Set(const char* key, const char* value, int flags = 0) noexcept;

			/**
			 * @brief Looks up @p key.
			 * @param key Entry key.
			 * @return Value pointer, or nullptr.
			 */
			const char* Value(const char* key) const noexcept;

			/**
			 * @brief Number of entries.
			 * @return Count, or 0.
			 */
			int Count() const noexcept;

		private:
			/**
			 * @brief Frees the dictionary (`av_dict_free`).
			 */
			void Free() noexcept override;

			using AVPointer<::AVDictionary>::Get;
	};

	extern template class STORMBYTE_MULTIMEDIA_PUBLIC AVPointer<::AVDictionary>;
}

/**
 * @brief Conditional provider contract: dictionary storage is released through FFmpeg out-of-line.
 * @note Multimedia, Base and FFmpeg must remain loaded with compatible ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::Dictionary);

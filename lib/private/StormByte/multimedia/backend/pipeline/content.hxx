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

#include <StormByte/multimedia/type.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/pointers.hxx>

#include <string>

extern "C" {
	struct AVFrame;
}

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @class Content
	 * @brief Strategy that adjusts libav extras after a payload swap.
	 *
	 * Works only on @c ::AVFrame pointers. It does not see
	 * @ref StormByte::Multimedia::Pipeline::Frame. Leaves live under
	 * @c Detail::Content (video, audio, passthrough). The backend
	 * Frame holder calls @ref For and @ref Put, then Reset and
	 * BindProperties.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Content {
		public:
			/**
			 * @brief Destructor.
			 */
			virtual ~Content() noexcept = default;

			/**
			 * @brief Copy constructor.
			 * @param other Unused. Strategies are not copied.
			 */
			Content(const Content& other) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Unused. Strategies are not moved.
			 */
			Content(Content&& other) noexcept = delete;

			/**
			 * @brief Copy assignment.
			 * @param other Unused.
			 * @return *this.
			 */
			Content& operator=(const Content& other) = delete;

			/**
			 * @brief Move assignment.
			 * @param other Unused.
			 * @return *this.
			 */
			Content& operator=(Content&& other) noexcept = delete;

			/**
			 * @brief Leaf for a media @p type.
			 * @param type Video, Audio, Subtitle, or other.
			 * @return Owned strategy. Never null.
			 *
			 * Video and Audio leaves land under Detail::Content.
			 * Until those files exist every type is passthrough.
			 */
			static StormByte::Safe::Unique<Content> For(StormByte::Multimedia::Type type) noexcept;

			/**
			 * @brief Make extras on @p after match the card of @p before.
			 * @param before Raw frame that was in the holder. May be nullptr.
			 * @param after Raw frame the leaf produced. May be nullptr.
			 *
			 * Does not take ownership. Does not Fail the tube.
			 * Coupled extras that cannot be remapped are dropped and
			 * described by @ref Warning.
			 */
			virtual void Put(const ::AVFrame* before, ::AVFrame* after) noexcept = 0;

			/**
			 * @brief Why coupled extras were dropped, if they were.
			 * @return Empty when extras were kept or remapped.
			 */
			const std::string& Warning() const noexcept;

		protected:
			/**
			 * @brief Empty strategy. Leaves construct this.
			 */
			Content() noexcept = default;

			std::string m_warning;	///< Drop reason; empty if none
	};
}

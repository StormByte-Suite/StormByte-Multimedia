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

#include <StormByte/multimedia/ffmpeg/AVChannelLayout.hxx>
#include <StormByte/multimedia/ffmpeg/AVRational.hxx>

extern "C" {
	#include <libavutil/channel_layout.h>
	#include <libavutil/mem.h>
	#include <libavutil/rational.h>
}

/**
 * @file convert.hxx
 * @brief cxx-only bridge between RAII wrappers and libav value types.
 *
 * Include from .cxx after the FFmpeg C headers. Not installed.
 */

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
			 * @brief Friend of AVChannelLayout. Touches the owned C pointer.
			 */
			struct Convert {
				/**
				 * @brief Const C layout.
				 * @param layout Wrapper.
				 * @return Pointer, or nullptr.
				 */
				static const ::AVChannelLayout* Raw(const AVChannelLayout& layout) noexcept {
					return layout.m_raw;
				}

				/**
				 * @brief Mutable C layout.
				 * @param layout Wrapper.
				 * @return Pointer, or nullptr.
				 */
				static ::AVChannelLayout* Raw(AVChannelLayout& layout) noexcept {
					return layout.m_raw;
				}

				/**
				 * @brief Deep-copies a C layout into a wrapper.
				 * @param src C layout.
				 * @return Wrapper, or empty on failure.
				 */
				static AVChannelLayout From(const ::AVChannelLayout& src) noexcept {
					AVChannelLayout out;
					out.Ensure();
					if (!out.m_raw)
						return out;
					if (av_channel_layout_copy(out.m_raw, &src) < 0)
						out.Free();
					return out;
				}
			};

			/**
			 * @brief Public / backend rational to C `AVRational`.
			 * @param rational Wrapper.
			 * @return C aggregate.
			 */
			inline ::AVRational ToRaw(const AVRational& rational) noexcept {
				return { rational.num, rational.den };
			}

			/**
			 * @brief C `AVRational` to public / backend rational.
			 * @param rational C aggregate.
			 * @return Wrapper.
			 */
			inline AVRational FromRaw(::AVRational rational) noexcept {
				return AVRational(rational.num, rational.den);
			}

			/**
			 * @brief Const C channel layout, or nullptr.
			 * @param layout Wrapper.
			 * @return Pointer, or nullptr.
			 */
			inline const ::AVChannelLayout* ToRaw(const AVChannelLayout& layout) noexcept {
				return Convert::Raw(layout);
			}

			/**
			 * @brief Mutable C channel layout, or nullptr.
			 * @param layout Wrapper.
			 * @return Pointer, or nullptr.
			 */
			inline ::AVChannelLayout* ToRaw(AVChannelLayout& layout) noexcept {
				return Convert::Raw(layout);
			}

			/**
			 * @brief C channel layout to RAII wrapper (copy).
			 * @param layout C layout.
			 * @return Wrapper.
			 */
			inline AVChannelLayout FromRaw(const ::AVChannelLayout& layout) noexcept {
				return Convert::From(layout);
			}

			/**
			 * @brief Optional C channel layout to RAII wrapper (copy).
			 * @param layout C layout, or nullptr.
			 * @return Wrapper, or empty.
			 */
			inline AVChannelLayout FromRaw(const ::AVChannelLayout* layout) noexcept {
				return layout ? Convert::From(*layout) : AVChannelLayout{};
			}
		}
	}
}

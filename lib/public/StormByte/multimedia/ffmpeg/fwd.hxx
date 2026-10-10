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

#include <StormByte/type_traits/safe.hxx>

/**
 * @file fwd.hxx
 * @brief Incomplete FFmpeg types for RAII headers. Do not include libav* here.
 *
 * The C tag `AVChannelLayout` is `::AVChannelLayout`. The RAII class is
 * `StormByte::Multimedia::FFmpeg::AVChannelLayout`. Always qualify
 * the C type with `::` inside the FFmpeg namespace.
 */

extern "C" {
	/**
	 * @brief Incomplete FFmpeg audio FIFO handle.
	 */
	struct AVAudioFifo;

	/**
	 * @brief Incomplete FFmpeg bitstream-filter context.
	 */
	struct AVBSFContext;

	/**
	 * @brief Incomplete FFmpeg C channel layout.
	 */
	struct AVChannelLayout;

	/**
	 * @brief Incomplete FFmpeg codec descriptor.
	 */
	struct AVCodec;

	/**
	 * @brief Incomplete FFmpeg codec context.
	 */
	struct AVCodecContext;

	/**
	 * @brief Incomplete FFmpeg C codec parameters.
	 */
	struct AVCodecParameters;

	/**
	 * @brief Incomplete FFmpeg dictionary.
	 */
	struct AVDictionary;

	/**
	 * @brief Incomplete FFmpeg format context.
	 */
	struct AVFormatContext;

	/**
	 * @brief Incomplete FFmpeg C frame.
	 */
	struct AVFrame;

	/**
	 * @brief Incomplete FFmpeg filter context.
	 */
	struct AVFilterContext;

	/**
	 * @brief Incomplete FFmpeg C filter graph.
	 */
	struct AVFilterGraph;

	/**
	 * @brief Incomplete FFmpeg frame side data.
	 */
	struct AVFrameSideData;

	/**
	 * @brief Incomplete FFmpeg I/O context.
	 */
	struct AVIOContext;

	/**
	 * @brief Incomplete FFmpeg C packet.
	 */
	struct AVPacket;

	/**
	 * @brief Incomplete FFmpeg C stream.
	 */
	struct AVStream;

	/**
	 * @brief Incomplete FFmpeg subtitle.
	 */
	struct AVSubtitle;

	/**
	 * @brief Incomplete FFmpeg audio resampler context.
	 */
	struct SwrContext;

	/**
	 * @brief Incomplete FFmpeg video scaler context.
	 */
	struct SwsContext;
}

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
			 * @brief Forward declaration of the codec-parameter wrapper.
			 */
			class AVCodecParameters;
		}
	}
}

/**
 * @brief Declare the private codec-parameter wrapper conditionally DLL-safe.
 * @note Its provider owns FFmpeg allocations; Multimedia and Base must remain loaded.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVCodecParameters);

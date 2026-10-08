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

#include <StormByte/multimedia/ffmpeg/AVStream.hxx>
#include <StormByte/multimedia/ffmpeg/typedefs.hxx>
#include <StormByte/safe/vector.hxx>

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Backend-only FFmpeg wrapper result and stream aliases.
 */
namespace StormByte::Multimedia::FFmpeg {
	class AVBSF;
	class AVDecoder;
	class AVEncoder;
	class AVFormatContext;

	/**
	 * @typedef ExpectedAVFormatContext
	 * @brief Backend result of opening an FFmpeg input context.
	 */
	using ExpectedAVFormatContext = StormByte::Expected<AVFormatContext, FFmpeg::DecoderError>;

	/**
	 * @typedef ExpectedAVDecoder
	 * @brief Backend result of opening an FFmpeg decoder.
	 */
	using ExpectedAVDecoder = StormByte::Expected<AVDecoder, FFmpeg::DecoderError>;

	/**
	 * @typedef ExpectedAVEncoder
	 * @brief Backend result of opening an FFmpeg encoder.
	 */
	using ExpectedAVEncoder = StormByte::Expected<AVEncoder, FFmpeg::EncoderError>;

	/**
	 * @typedef ExpectedAVBSF
	 * @brief Backend result of creating an FFmpeg bitstream filter.
	 */
	using ExpectedAVBSF = StormByte::Expected<AVBSF, FFmpeg::BSFError>;

	/**
	 * @typedef Streams
	 * @brief Ordered borrowed stream views; their format context must outlive them.
	 */
	using Streams = StormByte::Safe::Vector<AVStream>;
}

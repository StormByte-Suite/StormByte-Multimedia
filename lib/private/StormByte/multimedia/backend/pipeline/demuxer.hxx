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

#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/visibility.h>

#include <StormByte/multimedia/ffmpeg/AVRational.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>

#include <memory>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	using FFmpeg::AVRational;
	/**
	 * @class Demuxer
	 * @brief Single demux backend behind the public Demuxer.
	 *
	 * Opens whatever libavformat accepts. Not split per container.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Demuxer {
		public:
			/**
			 * @brief Empty format context.
			 */
			Demuxer() noexcept;

			/**
			 * @brief Closes the format context.
			 */
			~Demuxer() noexcept;

			Demuxer(const Demuxer& other) = delete;
			Demuxer& operator=(const Demuxer& other) = delete;
			Demuxer(Demuxer&& other) noexcept = delete;
			Demuxer& operator=(Demuxer&& other) noexcept = delete;

			/**
			 * @brief Whether the format context is open.
			 * @return true after a successful Open.
			 */
			bool IsOpen() const noexcept;

			/**
			 * @brief Opens the origin held by @p owner.
			 * @param owner Public demuxer.
			 * @return false if owner.Fail() was called.
			 */
			bool Open(StormByte::Multimedia::Pipeline::Demuxer& owner) noexcept;

			/**
			 * @brief Reads one compressed packet.
			 * @param owner Public demuxer.
			 * @return Packet, or empty at EOF / after Fail.
			 *
			 * During Measure, packets whose track is not in
			 * owner.m_measureTracks are skipped (not treated as EOF).
			 */
			StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet> Read(
				StormByte::Multimedia::Pipeline::Demuxer& owner) noexcept;

			/**
			 * @brief Seeks the origin to the start. Hoppers stay open.
			 * @param owner Public demuxer.
			 * @return false if owner.Fail() was called.
			 */
			bool Rewind(StormByte::Multimedia::Pipeline::Demuxer& owner) noexcept;

			/**
			 * @brief Opens a decode backend for @p decoder.Index().
			 * @param owner Public demuxer.
			 * @param decoder Destination decoder.
			 * @return Opened leaf, or empty after decoder.Fail().
			 */
			StormByte::Safe::Unique<Decoder> OpenDecoder(
				StormByte::Multimedia::Pipeline::Demuxer& owner,
				StormByte::Multimedia::Pipeline::Decoder& decoder) noexcept;

			/**
			 * @brief Clones codecpar and time base of origin stream @p index.
			 * @param index Source stream index.
			 * @param params Owned AVCodecParameters on success.
			 * @param timeBase Source time base.
			 * @return false if closed or @p index is missing.
			 */
			bool CloneStream(int index, ::AVCodecParameters*& params, AVRational& timeBase) noexcept;

			/**
			 * @brief Closes the format context.
			 */
			void Close() noexcept;

		private:
			class Context;
			StormByte::Safe::Unique<Context> m_ctx;
	};
}

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

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	class AVFrame;

	/**
	 * @class AudioFifo
	 * @brief RAII `AVAudioFifo` for sample buffering (loudnorm / encoder).
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE AudioFifo: public AVPointer<::AVAudioFifo> {
		public:
			/**
			 * @brief Move constructor. Transfers the fifo.
			 * @param other Source fifo; left empty.
			 */
			AudioFifo(AudioFifo&& other) noexcept = default;

			/**
			 * @brief Destructor. Frees the `AVAudioFifo`.
			 */
			~AudioFifo() noexcept override;

			/**
			 * @brief Move assignment. Frees *this, then takes @p other.
			 * @param other Source fifo; left empty.
			 * @return *this.
			 */
			AudioFifo& operator=(AudioFifo&& other) noexcept = default;

			/**
			 * @brief Whether a fifo is allocated.
			 * @return true if @ref Write / @ref Read can run.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief Allocates a fifo. Empty wrapper on failure.
			 * @param sample_fmt `AVSampleFormat` as int.
			 * @param channels Channel count.
			 * @param nb_samples Initial capacity in samples.
			 * @return Open fifo, or empty on failure.
			 */
			static AudioFifo Open(int sample_fmt, int channels, int nb_samples) noexcept;

			/**
			 * @brief Samples currently stored.
			 * @return Sample count, or 0.
			 */
			int Size() const noexcept;

			/**
			 * @brief Free sample slots.
			 * @return Remaining capacity, or 0.
			 */
			int Space() const noexcept;

			/**
			 * @brief Appends @p src samples to the fifo.
			 * @param src Source audio frame.
			 * @return false on failure.
			 */
			bool Write(const AVFrame& src) noexcept;

			/**
			 * @brief Reads @p nb_samples into @p dst (already allocated).
			 * @param dst Destination audio frame.
			 * @param nb_samples Samples to read.
			 * @return false on failure.
			 */
			bool Read(AVFrame& dst, int nb_samples) noexcept;

			/**
			 * @brief Grows the fifo so it can hold at least @p nb_samples.
			 * @param nb_samples Minimum capacity in samples.
			 * @return false on failure.
			 */
			bool Realloc(int nb_samples) noexcept;

		private:
			/**
			 * @brief Adopts an allocated fifo.
			 * @param fifo libavutil fifo, or nullptr.
			 */
			explicit AudioFifo(::AVAudioFifo* fifo) noexcept;

			/**
			 * @brief Deleted. Use @ref Open; an empty fifo is not useful.
			 */
			AudioFifo() = delete;

			/**
			 * @brief Frees the fifo (`av_audio_fifo_free`).
			 */
			void Free() noexcept override;

			using AVPointer<::AVAudioFifo>::Get;
	};

	extern template class STORMBYTE_MULTIMEDIA_PRIVATE AVPointer<::AVAudioFifo>;
}

/**
 * @brief Conditional provider contract: FIFO storage is released through FFmpeg out-of-line.
 * @note Multimedia, Base and FFmpeg must remain loaded with compatible ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AudioFifo);

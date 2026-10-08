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

#include <StormByte/logger/log.hxx>
#include <StormByte/multimedia/ffmpeg/AVFilterGraph.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

#include <memory>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Audio
 * @brief Audio process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Audio {
	/**
	 * @class Rubberband
	 * @brief Time-stretch and pitch-shift via libavfilter `rubberband`. Process leaf.
	 *
	 * Attach with @c job.Audio(in).Filter<Rubberband>(log).
	 *
	 * @par What it is for
	 * Change duration without changing pitch, or pitch without
	 * changing duration (or both). Typical jobs: fit a track
	 * to a picture cut, retune a source a few cents, slow
	 * speech a notch without the chipmunk effect. It is not
	 * @ref Resample : sample rate stays put; only tempo/pitch
	 * move. Hardware does not apply to audio.
	 *
	 * @par Algorithm
	 * librubberband through @ref FFmpeg::AVFilterGraph
	 * (`abuffer → rubberband → abuffersink`). Tempo scales
	 * duration. Pitch scales frequency. Both default to 1
	 * (identity). The graph owns the look-ahead; this leaf
	 * does not Hold.
	 *
	 * Early units may not leave the sink (`EAGAIN`).
	 * @ref Process then returns without @ref Filter::FFmpeg::Save.
	 * PTS comes from the sink frame.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the sink frame.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Process
	 * @see StormByte::Multimedia::FFmpeg::AVFilterGraph
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Rubberband: public Filter::Process {
		public:
			/**
			 * @brief Rubber Band tempo / pitch (`rubberband`).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param tempo Tempo scale. Empty → 1. Range 0.01–100.
			 * @param pitch Pitch scale. Empty → 1. Range 0.01–100.
			 */
			Rubberband(Safe::Shared<StormByte::Logger::Log> log,
				Safe::Optional<double> tempo = {},
				Safe::Optional<double> pitch = {}) noexcept;

			/**
			 * @brief Copy construction is disabled.
			 * @param other Source filter.
			 */
			Rubberband(const Rubberband& other) = delete;

			/**
			 * @brief Move construction is disabled.
			 * @param other Source filter.
			 */
			Rubberband(Rubberband&& other) noexcept = delete;

			/**
			 * @brief Releases the provider-owned graph.
			 */
			~Rubberband() noexcept override;

			/**
			 * @brief Copy assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Rubberband& operator=(const Rubberband& other) = delete;

			/**
			 * @brief Move assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Rubberband& operator=(Rubberband&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Audio.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Drops the cached graph.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Resets the graph before the first frame.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Pushes one audio frame through `rubberband` and Save.
			 * @param frame Current pipeline unit. Non-audio is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

		private:
			/**
			 * @brief Builds the avfilter chain.
			 * @return `rubberband=...` for @ref FFmpeg::AVFilterGraph::Ensure.
			 */
			Safe::String Chain() const noexcept;

			Safe::Optional<double> m_tempoIn;	///< Caller tempo, or empty
			Safe::Optional<double> m_pitchIn;	///< Caller pitch, or empty

			StormByte::Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< FFmpeg graph context.
	};
}

/**
 * @brief Requires compatible C++ ABI and loaded Multimedia, Base and Logger providers.
 * @note Private graph ownership is allocated and released only by Multimedia.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Audio::Rubberband);

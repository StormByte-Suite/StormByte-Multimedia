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
#include <StormByte/multimedia/property/channel_layout.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

#include <memory>
#include <string_view>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Audio
 * @brief Audio process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Audio {
	/**
	 * @class Downmix
	 * @brief Reduce channel count via libavfilter `aformat` (then `pan`). Process leaf.
	 *
	 * Attach with @c job.Audio(in).Filter<Downmix>(log, layout).
	 *
	 * @par What it is for
	 * Delivery that has fewer speakers than the source
	 * (7.1→5.1, 5.1→stereo). Target is any
	 * @ref Property::ChannelLayout, not a fixed stereo.
	 * Same channel **count** is a no-op (layout rename is
	 * not this leaf). Fewer source channels than wanted
	 * is @ref Filter::FFmpeg::Fail — use @ref Upmix.
	 *
	 * @par Do not stack
	 * Not with @ref Upmix on the same stretch. After
	 * @ref Resample is fine; before @ref Loudnorm so the
	 * meter sees the delivered layout.
	 *
	 * @par LFE
	 * `aformat` first so a standard layout keeps LFE when
	 * the target has one. Fallback
	 * `pan=…|FL=FL+0.707*LFE|FR=FR+0.707*LFE` folds LFE
	 * into the front pair. That is not a cinema 5.1 mix.
	 *
	 * @par Names
	 * mono, stereo, 2.1, 3.0, 4.0, quad, 5.0, 5.1, 6.1,
	 * 7.1, 7.1(wide), octagonal, 22.2.
	 * @ref Property::ChannelLayout::Unknown Fails.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save; BindProperties rewrites
	 * Frame::Audio. EAGAIN = wait. @ref Eof flushes.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Downmix: public Filter::Process {
		public:
			/**
			 * @brief Downmix toward @p target.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param target Destination speaker layout (not a fixed stereo).
			 */
			Downmix(Safe::Shared<StormByte::Logger::Log> log,
				Property::ChannelLayout target) noexcept;

			/**
			 * @brief Copy is not allowed. The graph is bound to one tube.
			 * @param other Source filter.
			 */
			Downmix(const Downmix& other) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the mounted leaf.
			 * @param other Source filter.
			 */
			Downmix(Downmix&& other) noexcept = delete;

			/**
			 * @brief Drops the cached graph.
			 */
			~Downmix() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Downmix& operator=(const Downmix& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Downmix& operator=(Downmix&& other) noexcept = delete;

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
			 * @brief Pushes one audio frame through `aformat`/`pan` and Save.
			 * @param frame Current pipeline unit. Non-audio is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Drains frames still held by the graph, if any.
			 */
			void Eof() noexcept override;

		private:
			/**
			 * @brief FFmpeg `aformat` / `pan` layout token for @ref m_target.
			 * @return Token, or empty when the layout is unknown.
			 * @note Borrows a string literal valid while Multimedia remains loaded.
			 */
			std::string_view LayoutName() const noexcept;

			/**
			 * @brief Preferred chain: `aformat=channel_layouts=…`.
			 * @return Filterchain, or empty if @ref LayoutName is empty.
			 */
			Safe::String AformatChain() const noexcept;

			/**
			 * @brief Fallback chain: fold LFE into FL/FR via `pan`.
			 * @return Filterchain, or empty if @ref LayoutName is empty.
			 */
			Safe::String PanChain() const noexcept;

			/**
			 * @brief Opens or reuses @ref m_graph for @p src and @p chain.
			 * @param src Model audio frame.
			 * @param chain Borrowed avfilter chain, valid throughout this call; not retained.
			 * @return false if the graph could not be (re)opened.
			 */
			bool EnsureGraph(const StormByte::Multimedia::FFmpeg::AVFrame& src,
				std::string_view chain) noexcept;

			Property::ChannelLayout m_target;	///< Destination layout

			std::unique_ptr<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< Reused graph

			bool m_pan = false;	///< true after LFE pan fallback
	};
}

/**
 * @brief Requires compatible C++ ABI and loaded Multimedia, Base and Logger providers.
 * @note Private graph ownership is allocated and released only by Multimedia.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Audio::Downmix);

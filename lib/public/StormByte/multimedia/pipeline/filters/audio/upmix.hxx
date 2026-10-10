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
	 * @class Upmix
	 * @brief Raise channel count toward a target layout. Process leaf.
	 *
	 * Attach with @c job.Audio(in).Filter<Upmix>(log, layout).
	 *
	 * @par What it is for
	 * Source narrower than the delivery (stereo→5.1,
	 * 5.1→7.1). Best-effort surround, especially LFE:
	 * `surround` extracts a low band when the target has
	 * LFE; it is **not** a studio 5.1 that was mixed that
	 * way. Same count = no-op. More source channels than
	 * wanted = Fail (that is @ref StormByte::Multimedia::Pipeline::Filter::Audio::Downmix).
	 *
	 * @par Do not stack
	 * Not with @ref StormByte::Multimedia::Pipeline::Filter::Audio::Downmix on the same stretch. Before
	 * @ref StormByte::Multimedia::Pipeline::Filter::Audio::Loudnorm. After @ref StormByte::Multimedia::Pipeline::Filter::Audio::Resample is fine.
	 *
	 * @par Algorithm
	 * Try `surround` (chl_in from
	 * @ref StormByte::Multimedia::FFmpeg::AVChannelLayout::Describe, LFE band
	 * when the target has LFE), then `aformat`, then
	 * `pan` with a folded LFE. Hardware N/A.
	 *
	 * @par Mutation
	 * Save + BindProperties. EAGAIN = wait. @ref StormByte::Multimedia::Pipeline::Filter::Audio::Upmix::Eof flushes.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Upmix: public Filter::Process {
		public:
			/**
			 * @brief Upmix toward @p target.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param target Destination speaker layout (not a fixed 5.1).
			 */
			Upmix(Safe::Shared<StormByte::Logger::Log> log,
					Property::ChannelLayout target) noexcept;

			/**
			 * @brief Copy is not allowed. The graph is bound to one tube.
			 * @param other Source filter.
			 */
			Upmix(const Upmix& other) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the mounted leaf.
			 * @param other Source filter.
			 */
			Upmix(Upmix&& other) noexcept = delete;

			/**
			 * @brief Drops the cached graph.
			 */
			~Upmix() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Upmix& operator=(const Upmix& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Upmix& operator=(Upmix&& other) noexcept = delete;

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
			 * @brief Pushes one audio frame through surround/aformat/pan and Save.
			 * @param frame Current pipeline unit. Non-audio is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Drains frames still held by the graph, if any.
			 */
			void Eof() noexcept override;

		private:
			/**
			 * @brief How the current graph was built.
			 */
			enum class Mode: std::uint8_t {
				None,			///< No graph yet.
				Surround,		///< `surround` with LFE band.
				Aformat,		///< `aformat=channel_layouts=`.
				Pan			///< `pan` fronts + summed LFE.
			};

			/**
			 * @brief FFmpeg layout token for @ref StormByte::Multimedia::Pipeline::Filter::Audio::Upmix::m_target.
			 * @return Token, or empty when the layout is unknown.
			 * @note Borrows a string literal valid while Multimedia remains loaded.
			 */
			std::string_view LayoutName() const noexcept;

			/**
			 * @brief Whether @ref StormByte::Multimedia::Pipeline::Filter::Audio::Upmix::m_target includes an LFE speaker.
			 * @return true for 2.1 / 5.1 / 6.1 / 7.1 / 7.1(wide) / 22.2.
			 */
			bool TargetHasLfe() const noexcept;

			/**
			 * @brief Preferred chain: `surround=chl_in=…:chl_out=…:lfe=…`.
			 * @param src Model frame (Describe for chl_in).
			 * @return Filterchain, or empty if @ref StormByte::Multimedia::Pipeline::Filter::Audio::Upmix::LayoutName is empty.
			 */
			Safe::String SurroundChain(const StormByte::Multimedia::FFmpeg::AVFrame& src) const noexcept;

			/**
			 * @brief Second chain: `aformat=channel_layouts=…`.
			 * @return Filterchain, or empty if @ref StormByte::Multimedia::Pipeline::Filter::Audio::Upmix::LayoutName is empty.
			 */
			Safe::String AformatChain() const noexcept;

			/**
			 * @brief Last chain: keep FL/FR, synthesize LFE from the sum.
			 * @return Filterchain, or empty if @ref StormByte::Multimedia::Pipeline::Filter::Audio::Upmix::LayoutName is empty.
			 */
			Safe::String PanChain() const noexcept;

			/**
			 * @brief Opens or reuses @ref StormByte::Multimedia::Pipeline::Filter::Audio::Upmix::m_graph for @p src and @p chain.
			 * @param src Model audio frame.
			 * @param chain Borrowed avfilter chain, valid throughout this call; not retained.
			 * @return false if the graph could not be (re)opened.
			 */
			bool EnsureGraph(const StormByte::Multimedia::FFmpeg::AVFrame& src,
					std::string_view chain) noexcept;

			Property::ChannelLayout m_target;	///< Destination layout
			StormByte::Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< FFmpeg graph context.
			Mode m_mode = Mode::None;		///< Active fallback step
	};
}

/**
 * @brief Requires compatible C++ ABI and loaded Multimedia, Base and Logger providers.
 * @note Private graph ownership is allocated and released only by Multimedia.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Audio::Upmix);

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

#include <cstdint>
#include <string>
#include <string_view>

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	class AVFrame;

	/**
	 * @class AVFilterGraph
	 * @brief RAII `AVFilterGraph` with a single video **or** audio in/out.
	 *
	 * @par Video
	 * Builds @c buffer → &lt;graph&gt; → @c buffersink. @p graph is an
	 * avfilter filterchain (e.g. @c "cas=strength=0.4"). One input
	 * pad only; filters that need a @c ref pad fail @ref Open.
	 *
	 * @par Audio
	 * Builds @c abuffer → &lt;graph&gt; → @c abuffersink. The model
	 * frame must have sample rate, sample format and a channel
	 * layout (@ref AVFrame::SampleRate, @ref AVFrame::Format,
	 * @ref AVFrame::ChannelLayout). Same one-pad rule.
	 *
	 * @par Selection
	 * @ref Open treats a frame with width and height &gt; 0 as
	 * video. Otherwise a frame with sample rate and channel count
	 * &gt; 0 is audio. Hardware video frames fail. Do not mix
	 * media on a reused wrapper: @ref Ensure rebuilds.
	 *
	 * Reuse the same wrapper across frames of the same geometry
	 * (video: size+format; audio: rate+format+layout) and chain
	 * via @ref Ensure. Do not call av_*.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC AVFilterGraph: public AVPointer<::AVFilterGraph> {
		public:
			/**
			 * @brief Move constructor. Transfers the graph.
			 * @param other Source graph; left empty.
			 */
			AVFilterGraph(AVFilterGraph&& other) noexcept;

			/**
			 * @brief Destructor. Frees the graph and its filters.
			 */
			~AVFilterGraph() noexcept override;

			/**
			 * @brief Move assignment. Frees *this, then takes @p other.
			 * @param other Source graph; left empty.
			 * @return *this.
			 */
			AVFilterGraph& operator=(AVFilterGraph&& other) noexcept;

			/**
			 * @brief Copy is not allowed. The graph owns libavfilter state.
			 */
			AVFilterGraph(const AVFilterGraph&) = delete;

			/**
			 * @brief Copy assignment is not allowed.
			 */
			AVFilterGraph& operator=(const AVFilterGraph&) = delete;

			/**
			 * @brief Whether the graph is configured.
			 * @return true if @ref Filter can run.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief Builds a graph for @p src. Empty wrapper on failure.
			 * @param src Model frame. Video: size, pixel format, SAR.
			 *        Audio: rate, sample format, channel layout.
			 *        Planes may be empty.
			 * @param graph Filterchain after @c buffer/@c abuffer,
			 *        before @c buffersink/@c abuffersink.
			 * @return Open graph, or empty on failure.
			 */
			static AVFilterGraph Open(const AVFrame& src, std::string_view graph) noexcept;

			/**
			 * @brief Rebuilds if geometry, format, layout or chain changed.
			 * @param src Model frame.
			 * @param graph Filterchain.
			 * @return false if the graph could not be (re)opened.
			 */
			bool Ensure(const AVFrame& src, std::string_view graph) noexcept;

			/**
			 * @brief Pushes @p src and pulls into @p dst (`KEEP_REF` on the source).
			 * @param src Source frame with buffers.
			 * @param dst Destination; unreferenced then filled by the sink.
			 * @return false on failure. Empty @p dst with true is EAGAIN.
			 */
			bool Filter(const AVFrame& src, AVFrame& dst) const noexcept;

			/**
			 * @brief Closes the source once and pulls one flushed frame into @p dst.
			 * @param dst Destination; unreferenced then filled by the sink.
			 * @return false on failure. Empty @p dst with true is EAGAIN/EOF.
			 *
			 * Call in a loop from a leaf @c Eof until @p dst is empty.
			 * The source is closed on the first call only.
			 */
			bool Flush(AVFrame& dst) noexcept;

		private:
			/**
			 * @brief Adopts an allocated graph.
			 * @param graph libavfilter graph, or nullptr.
			 */
			explicit AVFilterGraph(::AVFilterGraph* graph) noexcept;

			/**
			 * @brief Deleted. Use @ref Open.
			 */
			AVFilterGraph() = delete;

			/**
			 * @brief Frees the graph (`avfilter_graph_free`).
			 */
			void Free() noexcept override;

			using AVPointer<::AVFilterGraph>::Get;

			::AVFilterContext* m_src = nullptr;	///< buffer / abuffer (owned by the graph)
			::AVFilterContext* m_sink = nullptr;	///< buffersink / abuffersink (owned by the graph)
			int m_w = 0;							///< Cached video width
			int m_h = 0;							///< Cached video height
			int m_fmt = 0;							///< Cached pixel or sample format
			int m_rate = 0;							///< Cached audio sample rate
			int m_ch = 0;							///< Cached audio channel count
			std::uint64_t m_mask = 0;				///< Cached native channel mask
			std::string m_graph;					///< Cached filterchain
			bool m_closed = false;					///< source already closed
	};

	extern template class STORMBYTE_MULTIMEDIA_PUBLIC AVPointer<::AVFilterGraph>;
}

/**
 * @brief Conditional provider contract: graph and private cached text lifetimes stay in Multimedia.
 * @note Multimedia, Base and FFmpeg must remain loaded with compatible ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVFilterGraph);

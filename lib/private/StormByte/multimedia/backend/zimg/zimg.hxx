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

#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/visibility.h>

#include <zimg.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

/**
 * @namespace StormByte::Multimedia::Backend
 * @brief Internal backend facades.
 */
namespace StormByte::Multimedia::Backend {
	/**
	 * @class Zimg
	 * @brief RAII zimg filter graph. Not installed. Not a leaf API.
	 *
	 * Same shape as @ref StormByte::Multimedia::FFmpeg::Sws:
	 * @ref Open, @ref Ensure, @ref Scale.
	 * Packed RGB and hardware frames fail @ref Open.
	 * Scale copies color tags from the source.
	 * Tiles follow FFmpeg vf_zscale: source @c active_region, destination
	 * height is the strip, destination pointers are offset. Worker threads
	 * belong to this object and join in the destructor.
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Zimg {
		friend class FFmpeg::AVFrame;

		public:
			/**
			 * @brief Move constructor. Workers stay with *this.
			 * @param other Source; left empty.
			 */
			Zimg(Zimg&& other) noexcept;

			/**
			 * @brief Destructor. Stops workers and frees graphs.
			 */
			~Zimg() noexcept;

			/**
			 * @brief Move assignment.
			 * @param other Source; left empty.
			 * @return *this.
			 */
			Zimg& operator=(Zimg&& other) noexcept;

			Zimg(const Zimg&) = delete;
			Zimg& operator=(const Zimg&) = delete;

			/**
			 * @brief Whether a graph is selected for @ref Scale.
			 * @return true if @ref Scale can run.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief Builds a graph. Empty wrapper on failure.
			 * @param src Source frame (size, format, color).
			 * @param dst Destination frame (size, format).
			 * @param filter Resample kernel. @c Default leaves zimg defaults.
			 * @return Open scaler, or empty on failure.
			 */
			static Zimg Open(const FFmpeg::AVFrame& src, const FFmpeg::AVFrame& dst,
				FFmpeg::AVFrame::Resample filter) noexcept;

			/**
			 * @brief Selects or builds the graph for this geometry and kernel.
			 * @param src Source frame.
			 * @param dst Destination frame.
			 * @param filter Resample kernel.
			 * @return false if the graph could not be (re)opened.
			 */
			bool Ensure(const FFmpeg::AVFrame& src, const FFmpeg::AVFrame& dst,
				FFmpeg::AVFrame::Resample filter) noexcept;

			/**
			 * @brief Scales @p src into @p dst. Both must already have buffers.
			 * @param src Source frame.
			 * @param dst Destination frame.
			 * @return false on failure.
			 */
			bool Scale(const FFmpeg::AVFrame& src, FFmpeg::AVFrame& dst) const noexcept;

		private:
			/**
			 * @brief One destination strip (FFmpeg tile).
			 */
			struct Strip {
				zimg_filter_graph* graph = nullptr;	///< Graph for this strip
				std::vector<std::uint8_t> scratch;	///< tmp + 64-byte pad
				unsigned outTop = 0;				///< First destination row
				unsigned outHeight = 0;				///< Destination strip height
			};

			/**
			 * @brief Cached strips for one geometry/kernel.
			 */
			struct Slot {
				std::vector<Strip> strips;			///< FFmpeg-style tiles
				int srcW = 0;						///< Cached source width
				int srcH = 0;						///< Cached source height
				int srcFmt = 0;						///< Cached source format
				int dstW = 0;						///< Cached destination width
				int dstH = 0;						///< Cached destination height
				int dstFmt = 0;						///< Cached destination format
				int filter = 0;						///< Cached kernel
				unsigned subH = 0;					///< Destination chroma log2
			};

			/**
			 * @brief Empty scaler. Used by @ref Open and the per-thread
			 *        slot in @ref FFmpeg::AVFrame::ScaleTo.
			 */
			Zimg() noexcept;

			void Free() noexcept;
			void StopWorkers() noexcept;
			void EnsureWorkers(unsigned n) const noexcept;

			/**
			 * @brief Frees every graph in @p slot.
			 * @param slot Cache entry.
			 */
			static void FreeSlot(Slot& slot) noexcept;

			/**
			 * @brief Builds one FFmpeg-style strip graph.
			 * @param strip Destination strip.
			 * @param src Source frame.
			 * @param dst Destination frame.
			 * @param in Full source format (color already copied).
			 * @param out Full destination format (color already copied).
			 * @param filter Resample kernel.
			 * @param top First destination row.
			 * @param height Strip height.
			 * @return false on failure.
			 */
			static bool BuildStrip(Strip& strip, const FFmpeg::AVFrame& src,
				const FFmpeg::AVFrame& dst, zimg_image_format in, zimg_image_format out,
				FFmpeg::AVFrame::Resample filter, unsigned top, unsigned height) noexcept;

			/**
			 * @brief Builds strip graphs into @p slot.
			 * @param slot Cache entry to fill.
			 * @param src Source frame.
			 * @param dst Destination frame.
			 * @param filter Resample kernel.
			 * @return false on failure.
			 */
			static bool BuildSlot(Slot& slot, const FFmpeg::AVFrame& src,
				const FFmpeg::AVFrame& dst, FFmpeg::AVFrame::Resample filter) noexcept;

			/**
			 * @brief Runs one strip. Destination pointers are offset.
			 * @param strip Tile.
			 * @param srcBuf Full source buffers.
			 * @param dstBuf Full destination buffers.
			 * @param subH Destination chroma log2.
			 * @return false on failure.
			 */
			static bool ProcessStrip(const Strip& strip,
				const zimg_image_buffer_const& srcBuf, const zimg_image_buffer& dstBuf,
				unsigned subH) noexcept;

			void WorkerMain(unsigned id) const;

			std::vector<Slot> m_slots;					///< Graphs keyed by geometry/kernel
			Slot* m_current = nullptr;					///< Slot selected by the last Ensure

			mutable std::mutex m_mu;					///< Worker lock
			mutable std::condition_variable m_cv;		///< Worker wake / done
			mutable std::vector<std::thread> m_workers;	///< Persistent tile workers
			mutable unsigned m_epoch = 0;				///< Posted job generation
			mutable unsigned m_finished = 0;			///< Workers done this epoch
			mutable bool m_stop = false;				///< Destructor requested
			mutable const Slot* m_jobSlot = nullptr;	///< Slot for the posted Scale
			mutable zimg_image_buffer_const m_jobSrc{};	///< Posted source buffers
			mutable zimg_image_buffer m_jobDst{};		///< Posted destination buffers
			mutable std::vector<int> m_jobOk;			///< Per-strip result
	};
}

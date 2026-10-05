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
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/pipeline/filters/report.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/pointers.hxx>

#include <memory>
#include <utility>
#include <vector>

/**
 * @namespace StormByte::Multimedia::Pipeline
 * @brief Demux / decode / filter / encode / mux types.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Pipeline {
	class Filters;

	/**
	 * @class Route
	 * @brief Owns the filter chain of one origin track and two ends.
	 *
	 * Advanced API. Construct origin and destination as
	 * @c StormByte::Safe::Shared<Step>. Construct concrete stages with
	 * @c StormByte::Safe::Heap::MakeShared so ownership stays on Base's heap.
	 *
	 * Packet / BSF filters may sit between Demuxer and Remuxer.
	 * Frame / Process filters on that stretch fail at Close.
	 * Encode lanes use Frame and Packet filters between Decoder
	 * and Encoder.
	 *
	 * Analytics is not a process node. Close wires Process from
	 * origin to destination. Frame origins
	 * @ref Backend::Pipeline::Pipe::CloneTo the origin Pipe
	 * onto each Analytics (decode look). Packet origins spawn a
	 * Route-owned Decoder so Analytics still sees frames with
	 * @ref Producer::Decoder. Destination packet producers spawn
	 * a look Decoder from a CloneTo of dest Out. The last
	 * Analytics Pipe Out is @c StormByte::Buffer::Sink::Drain.
	 *
	 * Close is private: add Routes through @ref Filters and call
	 * @ref Filters::Close. CloneTo the origin before it emits.
	 *
	 * Muxer closed is not the end of this Route. Wait @ref Idle
	 * before @ref Reports.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Route {
		friend class Filters;

		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Route for one origin track and its two ends.
			 * @param track Origin stream index.
			 * @param origin Producer (Decoder / Demuxer). Must not be empty.
			 * @param destination Consumer (Encoder / Remuxer / Muxer). Must not be empty.
			 *
			 * Both ends retain Base-heap shared ownership. The Step dies when
			 * the last Route, Transcoder lane or caller drops it.
			 */
			Route(int track,
				StormByte::Safe::Shared<Step> origin,
				StormByte::Safe::Shared<Step> destination) noexcept;

			/**
			 * @brief Copy constructor.
			 * @param other Source route.
			 */
			Route(const Route& other) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Route to take.
			 */
			Route(Route&& other) noexcept = delete;

			/**
			 * @brief Destructor. Halts owned filters and look Decoders
			 *        while those objects are still complete.
			 */
			~Route() noexcept;

			/**
			 * @brief Copy assignment.
			 * @param other Source route.
			 * @return *this.
			 */
			Route& operator=(const Route& other) = delete;

			/**
			 * @brief Move assignment.
			 * @param other Route to take.
			 * @return *this.
			 */
			Route& operator=(Route&& other) noexcept = delete;

			/**
			 * @}
			 */

			/**
			 * @brief Origin track.
			 * @return Stream index passed to the constructor.
			 */
			int Track() const noexcept;

			/**
			 * @brief Takes ownership of @p filter, hooks it and launches it.
			 * @param filter Process, Packet or Analytics leaf.
			 * @return *this.
			 *
			 * A leaf that is not Process, Packet or Analytics fails
			 * the filter. Launch starts the worker; the worker waits
			 * on the Pipe until @ref Filters::Close binds a hopper.
			 */
			Route& Add(StormByte::Safe::Shared<Filter::FFmpeg> filter) noexcept;

			/**
			 * @brief Constructs a leaf of type @p T on this track.
			 * @tparam T Process, Packet or Analytics leaf.
			 * @tparam Args Constructor arguments.
			 * @param args Forwarded to @p T.
			 * @return *this.
			 */
			template<typename T, typename... Args>
			Route& Add(Args&&... args) noexcept {
				return Add(StormByte::Safe::Shared<Filter::FFmpeg>(
					StormByte::Safe::Heap::MakeShared<T>(std::forward<Args>(args)...)));
			}

			/**
			 * @brief Whether every owned worker has left Ready.
			 * @return true when all filters and look Decoders are
			 *         Stopped or Failed.
			 *
			 * False while Analytics is still pairing looks. Wait
			 * this before @ref Reports on a hand-built tube.
			 */
			bool Idle() const noexcept;

			/**
			 * @brief Reports of every kept filter.
			 * @return One entry per owned filter, in Add order.
			 *
			 * Call after @ref Idle.
			 */
			std::vector<Filter::Report> Reports() const noexcept;

		private:
			/**
			 * @brief Wires this track from the stored origin to destination.
			 *
			 * Private: @ref Filters is the only caller. O(1) at the
			 * ends. Process uses Pipe @c >> . Each Analytics gets
			 * its own CloneTo and dest look. Last Analytics Pipe Out is Drained.
			 */
			void Close() noexcept;

			/**
			 * @class Lane
			 * @brief Process chain and analytics chain of one @ref Kind.
			 */
			struct STORMBYTE_MULTIMEDIA_PRIVATE Lane {
				Filter::FFmpeg* FirstProcess = nullptr;
				Filter::FFmpeg* LastProcess = nullptr;

				/**
				 * @brief First process node of this lane.
				 * @return Process head, or null. Analytics is not in the tube.
				 */
				Filter::FFmpeg* First() const noexcept {
					return FirstProcess;
				}

				/**
				 * @brief Last process node of this lane.
				 * @return Process tail, or null. Analytics is not in the tube.
				 */
				Filter::FFmpeg* Last() const noexcept {
					return LastProcess;
				}
			};

			/**
			 * @brief Hooks @p filter onto @p lane in O(1).
			 * @param lane Process / analytics of one Kind.
			 * @param filter Node to append.
			 */
			void Hook(Lane& lane, Filter::FFmpeg& filter) noexcept;

			/**
			 * @brief Feeds @p analytics with the source look.
			 */
			void TapDecode(Step& origin, Filter::FFmpeg& analytics) noexcept;

			/**
			 * @brief Feeds @p analytics with the destination look.
			 */
			void TapEncode(Step& destination, Filter::FFmpeg& analytics) noexcept;

			/**
			 * @brief Wires an already-launched analytics leaf (global Add).
			 */
			void Observe(Filter::FFmpeg& analytics) noexcept;

			int m_track;													///< Origin stream index
			StormByte::Safe::Shared<Step> m_origin;							///< Producer retained through Base-heap shared ownership.

			StormByte::Safe::Shared<Step> m_destination;					///< Consumer retained through Base-heap shared ownership.
			Lane m_frames;													///< Frame process + analytics
			Lane m_packets;													///< Packet process + analytics
			std::vector<StormByte::Safe::Shared<Filter::FFmpeg>> m_filters;	///< Base-heap filter owners retained in insertion order.
			std::vector<Filter::FFmpeg*> m_analytics;						///< Per-stretch and observed globals
			std::vector<std::unique_ptr<Decoder>> m_looks;					///< Route-owned source and dest look decoders
	};
}

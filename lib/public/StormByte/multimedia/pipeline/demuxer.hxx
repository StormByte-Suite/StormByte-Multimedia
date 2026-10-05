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

#include <StormByte/buffer/fifo.hxx>
#include <StormByte/buffer/io/buffered_file_reader.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/progress.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/pointers.hxx>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia::Backend::Pipeline
	 * @brief Multimedia-owned pipeline stages and unit holders.
	 *
	 * @ingroup multimedia_pipeline
	 */
	namespace Multimedia {
		namespace Backend {
			class FileAvio;

			namespace Pipeline {
				class Decoder;
				class Demuxer;
			}

			namespace Pipeline::Detail::Worker {
				class Demux;
			}
		}

		/**
		 * @namespace StormByte::Multimedia::Pipeline
		 * @brief Demux / decode / filter / encode / mux types.
		 *
		 * @ingroup multimedia_pipeline
		 */
		namespace Pipeline {
			class Decoder;
			class Demuxer;
			class Filters;
			class Muxer;
			class Packet;
			class Plan;
			class Remuxer;

			/**
			 * @brief Binds one origin track of @p demuxer to @p decoder.
			 * @param demuxer Origin demuxer.
			 * @param decoder Destination decoder.
			 * @return @p decoder.
			 */
			STORMBYTE_MULTIMEDIA_PUBLIC Decoder& operator>>(Demuxer& demuxer, Decoder& decoder) noexcept;

			/**
			 * @class Demuxer
			 * @brief Reads interleaved compressed packets from the Plan reader.
			 *
			 * Only tracks listed in the bound @ref Plan are emitted. Binding
			 * a second Plan fails the stage. Input comes from the Plan's
			 * seekable @ref StormByte::Buffer::IO::BufferedLocationReader.
			 *
			 * With @ref Filter::ProcessTwoPasses filters, the required tracks
			 * are read for measurement first. Once measurement completes,
			 * the source is rewound and read again for the output pass.
			 *
			 * @ref Progress() returns a shared, read-only progress handle
			 * that remains usable after the pipeline has been destroyed.
			 * It includes filtering and output progress for the same job.
			 *
			 * Each packet receives a serial number within its origin track
			 * and part zero. This identifies source lineage, not frame count.
			 *
			 * @ingroup multimedia_pipeline
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Demuxer final: public Step {
				friend class Backend::Pipeline::Demuxer;
				friend class Backend::Pipeline::Detail::Worker::Demux;
				friend class Decoder;
				friend class Filters;
				friend class Muxer;
				friend Decoder& operator>>(Demuxer& demuxer, Decoder& decoder) noexcept;
				friend Demuxer& operator>>(class Plan&& plan, Demuxer& demuxer) noexcept;
				friend Demuxer& operator>>(StormByte::Safe::Shared<class Plan> plan, Demuxer& demuxer) noexcept;
				friend Muxer& operator>>(Demuxer& demuxer, Muxer& muxer) noexcept;
				friend Remuxer& operator>>(Demuxer& demuxer, Remuxer& remuxer) noexcept;

				public:
					/**
					 * @name Lifecycle
					 * @{
					 */

					/**
					 * @brief Demuxer. Launches; Setup waits for a Plan.
					 * @param log Shared logger. Empty pointer means no log.
					 */
					explicit Demuxer(StormByte::Safe::Shared<StormByte::Logger::Log> log) noexcept;

					/**
					 * @brief Launch with a clock already used by duration resolution.
					 * @param log Shared logger.
					 * @param progress Shared job clock; empty creates a fresh clock.
					 */
					Demuxer(StormByte::Safe::Shared<StormByte::Logger::Log> log,
						StormByte::Safe::Shared<class Progress> progress) noexcept;

					/**
					 * @brief Copy constructor.
					 * @param other Source demuxer.
					 */
					Demuxer(const Demuxer& other) = delete;

					/**
					 * @brief Move constructor.
					 * @param other Demuxer to take.
					 */
					Demuxer(Demuxer&& other) noexcept = delete;

					/**
					 * @brief Destructor. @ref Step::Join Halt s before backends die.
					 */
					~Demuxer() noexcept override;

					/**
					 * @brief Copy assignment.
					 * @param other Source demuxer.
					 * @return *this.
					 */
					Demuxer& operator=(const Demuxer& other) = delete;

					/**
					 * @brief Move assignment.
					 * @param other Demuxer to take.
					 * @return *this.
					 */
					Demuxer& operator=(Demuxer&& other) noexcept = delete;

					/**
					 * @}
					 */

					/**
					 * @brief true if open, not failed and not at EOF.
					 * @return Open and readable.
					 */
					explicit operator bool() const noexcept;

					/**
					 * @name Source
					 * @{
					 */

					/**
					 * @brief Whether the last read hit EOF.
					 * @return true at end of source of the Process pass.
					 *
					 * Measurement end-of-file is cleared before the output pass.
					 */
					bool Eof() const noexcept;

					/**
					 * @brief Presentation time of the last emitted packet.
					 * @return Pts, or empty until a packet with Pts arrives.
					 */
					StormByte::Safe::Optional<Property::Duration> Position() const noexcept;

					/**
					 * @brief Job clock for this tube (measure, analytics, All).
					 *
					 * Not Demuxer-only: Filters and the Muxer write the same
					 * object. Shared so the user can keep it after the tube
					 * dies. No public setters.
					 *
					 * @return Const shared handle. Never empty.
					 */
					Progress::Pointer Progress() const noexcept;

					/**
					 * @}
					 */

				private:
					using Step::Log;

					/**
					 * @brief Blocks until a Plan is bound or the stage must leave.
					 */
					void WaitForPlan() noexcept;

					/**
					 * @brief Latches a known duration into the clock if available.
					 *
					 * Called after a Plan is bound. Does not open a path and
					 * does not probe a File.
					 */
					void LatchDuration() noexcept;

					/**
					 * @brief Marks end of source. Called by the backend on EOF.
					 *
					 * If @ref Measuring, asks Filters::CloseMeasureSource and
					 * does not close hoppers. Process-pass EoF is permanent.
					 */
					void ReachedEof() noexcept;

					/**
					 * @brief Restricts the next read to @p tracks and keeps hoppers open.
					 * @param tracks Origin indexes that feed ProcessTwoPasses.
					 *
					 * Friend: @ref Filters::Close. Empty @p tracks is a no-op.
					 */
					void Measure(std::vector<int> tracks) noexcept;

					/**
					 * @brief Whether @ref Measure is active.
					 * @return true until @ref Rewind.
					 */
					bool Measuring() const noexcept;

					/**
					 * @brief Wake the measure-EoF Wait when measure has ended.
					 * @return true iff not @ref Measuring.
					 *
					 * Demux worker office. That Wait is not hopper Ready.
					 */
					bool WakeNow() const noexcept override;

					/**
					 * @brief Seeks the Plan reader to the start without closing hoppers.
					 * @return false after Fail.
					 *
					 * Ends @ref Measure: later reads follow the bound Plan.
					 * Friend: @ref Filters::FinishMeasure. Uses
					 * @ref StormByte::Buffer::IO::BufferedReader::Rewind.
					 */
					bool Rewind() noexcept;

					/**
					 * @brief Origin reader owned by the bound Plan.
					 * @return Seekable reader.
					 */
					const StormByte::Buffer::IO::BufferedLocationReader& Origin() const noexcept;

					/**
					 * @brief Origin reader owned by the bound Plan.
					 * @return Seekable reader.
					 */
					StormByte::Buffer::IO::BufferedLocationReader& Origin() noexcept;

					/**
					 * @brief Opens the decode backend for @p decoder.
					 * @param decoder Destination decoder.
					 * @return Backend, or empty after Fail.
					 */
					std::unique_ptr<Backend::Pipeline::Decoder> OpenDecoder(Decoder& decoder) noexcept;

					/**
					 * @brief Builds a public packet. Called from the backend.
					 *
					 * Assigns the next @ref Packet::Serial for @p track and
					 * @ref Packet::Part zero. This is pipe lineage, not a
					 * decoded-frame count. Logs the unit at LowLevel.
					 *
					 * @p backend must carry a deep copy of the origin
					 * @c AVCodecParameters (extradata included). Analytics
					 * packet-looks open a decoder from that stamp. A packet
					 * without parameters is not a look unit.
					 *
					 * @param track Origin stream index.
					 * @param type Media type stamped on the packet.
					 * @param payload Compressed bytes.
					 * @param pts Presentation time.
					 * @param dts Decode time.
					 * @param duration Packet duration.
					 * @param keyframe Whether this is a keyframe.
					 * @param backend Holder with codecpar. May be empty only
					 *        if no look will consume this packet.
					 * @return Public packet.
					 */
					Packet::PointerType Wrap(
						int track,
						Type type,
						StormByte::Buffer::FIFO payload,
						std::optional<Property::Duration> pts,
						std::optional<Property::Duration> dts,
						std::optional<Property::Duration> duration,
						bool keyframe,
						std::unique_ptr<Backend::Pipeline::Packet> backend) noexcept;

					std::unique_ptr<Backend::Pipeline::Demuxer> m_backend;				///< Format backend
					bool m_eof;															///< End of Process-pass source
					std::mutex m_planMutex;												///< Guards Plan wait
					std::condition_variable m_planPresent;								///< Woken when a Plan arrives
					std::atomic<std::int64_t> m_positionNs;								///< Last packet Pts, or -1
					std::unordered_map<int, std::uint64_t> m_nextSerial;				///< Next lineage id per origin track
					std::vector<int> m_measureTracks;									///< Tracks visible during measure
					bool m_measuring = false;											///< Measure pass active
					Filters* m_filters = nullptr;										///< Facade that started measure
					StormByte::Safe::Shared<class Progress> m_progress;				///< Shared tube clock retained through Base-heap ownership.
					Join m_join{*this};													///< Halt before other members die
			};
		}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Demuxer);

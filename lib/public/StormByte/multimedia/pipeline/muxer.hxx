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

#include <StormByte/size.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/multimedia/attachment.hxx>
#include <StormByte/multimedia/container.hxx>
#include <StormByte/multimedia/pipeline/progress.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/atomic.hxx>
#include <StormByte/safe/map.hxx>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/set.hxx>
#include <StormByte/safe/string.hxx>

#include <cstddef>
#include <cstdint>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Multimedia module of the StormByte suite.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Backend
		 * @brief Private backends.
		 */
		namespace Backend {
			/**
			 * @namespace StormByte::Multimedia::Backend::Pipeline
			 * @brief Multimedia-owned pipeline stages and unit holders.
			 */
			namespace Pipeline {
				/**
				 * @brief Backend mux stage.
				 */
				class Muxer;

				/**
				 * @namespace StormByte::Multimedia::Backend::Pipeline::Detail
				 * @brief Private pipeline implementation details.
				 */
				namespace Detail {
					/**
					 * @namespace StormByte::Multimedia::Backend::Pipeline::Detail::Worker
					 * @brief Pipeline execution workers.
					 */
					namespace Worker {
						/**
						 * @brief Mux execution worker.
						 */
						class Mux;
					}

					/**
					 * @namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer
					 * @brief Muxer implementation details.
					 */
					namespace Muxer {
						/**
						 * @namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg
						 * @brief FFmpeg muxer implementation details.
						 */
						namespace FFmpeg {
							/**
							 * @brief FFmpeg output container backend.
							 */
							class Container;
						}
					}
				}
			}
		}

		/**
		 * @namespace StormByte::Multimedia::Pipeline
		 * @brief Demux / decode / filter / encode / mux types.
		 *
		 * @ingroup multimedia_pipeline
		 */
		namespace Pipeline {
				/**
				 * @brief Demux stage supplying the remux origin.
				 */
			class Demuxer;

				/**
				 * @brief Encode stage supplying output packets.
				 */
			class Encoder;

				/**
				 * @brief Mux stage writing output packets.
				 */
			class Muxer;

				/**
				 * @brief Remux stage forwarding source packets.
				 */
			class Remuxer;

			/**
			 * @brief Reserves @p encoder as an output track of @p muxer.
			 * @param encoder Live encoder.
			 * @param muxer Destination.
			 * @return @p encoder.
			 *
			 * Binds the Plan on the first reservation. Opens the Plan
			 * writer for output. Releases setup when all muxable tracks
			 * required by the Plan have been reserved.
			 */
			STORMBYTE_MULTIMEDIA_PUBLIC Encoder& operator>>(Encoder& encoder, Muxer& muxer) noexcept;

			/**
			 * @brief Reserves @p remuxer as an output track of @p muxer.
			 * @param remuxer Live remuxer.
			 * @param muxer Destination.
			 * @return @p remuxer.
			 *
			 * Binds the Plan on the first reservation. Opens the Plan
			 * writer for output. Releases setup when all muxable tracks
			 * required by the Plan have been reserved.
			 */
			STORMBYTE_MULTIMEDIA_PUBLIC Remuxer& operator>>(Remuxer& remuxer, Muxer& muxer) noexcept;

			/**
			 * @brief Binds @p demuxer as remux origin and forwards Plan attachments.
			 *
			 * Required when any remux track is reserved. Without it the muxer
			 * cannot clone origin codec parameters. Also shares the tube
			 * @ref Progress clock. Opens the Plan writer if needed.
			 * There is no @c file >> muxer and no
			 * @c muxer >> path.
			 *
			 * @param demuxer Origin demuxer. Must outlive header write.
			 * @param muxer Destination.
			 * @return @p muxer.
			 */
			STORMBYTE_MULTIMEDIA_PUBLIC Muxer& operator>>(Demuxer& demuxer, Muxer& muxer) noexcept;

			/**
			 * @class Muxer
			 * @brief Writes interleaved packets to the Plan writer.
			 *
			 * Destination is the seekable @ref Plan::Writer. The writer's
			 * path extension determines the destination container.
			 *
			 * Does not write the container header until @ref Armed is true.
			 * @ref Armed is reserved muxable tracks versus the Plan, not a
			 * bound path. @ref Ready is Status Ready and @ref Armed.
			 *
			 * Shares the Demuxer's @ref Progress. Written packets advance
			 * output progress; completion includes the container trailer
			 * and flushing the destination writer.
			 *
			 * @ingroup multimedia_pipeline
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Muxer final: public Step {
				public:
					/**
					 * @name Lifecycle
					 * @{
					 */

					/**
					 * @brief Muxer. Destination is the Plan writer.
					 * @param log Shared logger. Empty pointer means no log.
					 * @note Starts execution immediately. Setup waits until encoder/remuxer
					 * connections reserve all muxable tracks required by the Plan, or the
					 * stage is stopped or fails.
					 */
					explicit Muxer(StormByte::Safe::Shared<StormByte::Logger::Log> log) noexcept;

					/**
					 * @brief Copy constructor (deleted).
					 * @param other Source muxer.
					 */
					Muxer(const Muxer& other) = delete;

					/**
					 * @brief Move constructor (deleted).
					 * @param other Source muxer.
					 */
					Muxer(Muxer&& other) noexcept = delete;

					/**
					 * @brief Destructor. @ref Step::Join Halts before backends die.
					 */
					~Muxer() noexcept override;

					/**
					 * @brief Copy assignment (deleted).
					 * @param other Source muxer.
					 * @return *this.
					 */
					Muxer& operator=(const Muxer& other) = delete;

					/**
					 * @brief Move assignment (deleted).
					 * @param other Source muxer.
					 * @return *this.
					 */
					Muxer& operator=(Muxer&& other) noexcept = delete;

					/**
					 * @}
					 */

					/**
					 * @brief true if the muxer has not failed and is Ready.
					 * @return Open and writable.
					 */
					explicit operator bool() const noexcept;

					/**
					 * @brief true after Finish flushed the trailer, or after Fail.
					 * @return Muxer will not accept more packets.
					 */
					bool Closed() const noexcept;

					/**
					 * @brief Presentation time of the last packet written.
					 * @return Pts of the last video packet, or of audio if no video
					 *         has been written yet.
					 */
					StormByte::Safe::Optional<Property::Duration> Position() const noexcept;

					/**
					 * @brief Destination container.
					 * @return Registry container from the bound Plan.
					 */
					const Container& Destination() const noexcept;

					/**
					 * @brief Whether every muxable Plan track has an output connection.
					 *
					 * Counts Video, Audio and Subtitle entries in @ref Plan::Tracks.
					 * Attachments are written with the header, not as packet streams.
					 * true when connected tracks equal that count. false with no Plan,
					 * or while @c operator>> is still running.
					 *
					 * Does not Fail. The owner of the graph (@ref Filters::Close
					 * or Transcoder) checks this after wiring and Fails the Muxer
					 * if it is still false.
					 *
					 * @return Arming state of the output graph.
					 */
					bool Armed() const noexcept;

					/**
					 * @brief Open finished without Fail or Stop, and @ref Armed.
					 * @return Step Status is Ready and every muxable Plan track is reserved.
					 */
					bool Ready() const noexcept override;

					/**
					 * @brief Maximum number of queued input packets.
					 *
					 * The first muxable Plan track determines the limit.
					 * Calling without a bound Plan aborts the process.
					 *
					 * @return Max queued packets. Never 0 after a live Plan.
					 */
					StormByte::Size InputCeiling() const noexcept override;

					/**
					 * @name Stream tags
					 * @{
					 */

					/**
					 * @brief Language tag for mux output @p output_index.
					 * @param output_index Mux destination order key.
					 * @return Tag, or empty.
					 */
					StormByte::Safe::Optional<StormByte::Safe::String> Language(int output_index) const noexcept;

					/**
					 * @brief Sets the language tag for mux output @p output_index.
					 * @param output_index Mux destination order key.
					 * @param language BCP-47 / ISO tag. Empty clears.
					 */
					void Language(int output_index, StormByte::Safe::String language) noexcept;

					/**
					 * @brief Title tag for mux output @p output_index.
					 * @param output_index Mux destination order key.
					 * @return Title, or empty.
					 */
					StormByte::Safe::Optional<StormByte::Safe::String> Title(int output_index) const noexcept;

					/**
					 * @brief Sets the title tag for mux output @p output_index.
					 * @param output_index Mux destination order key.
					 * @param title Stream title. Empty clears.
					 */
					void Title(int output_index, StormByte::Safe::String title) noexcept;

					/**
					 * @}
					 */

				private:
					/**
					 * @brief Allows the FFmpeg container backend to access mux state.
					 */
					friend class Backend::Pipeline::Detail::Muxer::FFmpeg::Container;

					/**
					 * @brief Allows the mux backend to access its owning stage.
					 */
					friend class Backend::Pipeline::Muxer;

					/**
					 * @brief Allows the mux worker to drive output and progress.
					 */
					friend class Backend::Pipeline::Detail::Worker::Mux;

					/**
					 * @brief Allows encoder connections to reserve output tracks.
					 * @param encoder Live encoder.
					 * @param muxer Destination.
					 * @return Connected encoder.
					 */
					friend Encoder& operator>>(Encoder& encoder, Muxer& muxer) noexcept;

					/**
					 * @brief Allows demuxer connections to bind the remux origin.
					 * @param demuxer Origin demuxer.
					 * @param muxer Destination.
					 * @return Destination muxer.
					 */
					friend Muxer& operator>>(Demuxer& demuxer, Muxer& muxer) noexcept;

					/**
					 * @brief Allows remuxer connections to reserve output tracks.
					 * @param remuxer Live remuxer.
					 * @param muxer Destination.
					 * @return Connected remuxer.
					 */
					friend Remuxer& operator>>(Remuxer& remuxer, Muxer& muxer) noexcept;

					/**
					 * @brief Keeps the inherited logger accessor private.
					 */
					using Step::Log;

					/**
					 * @brief Blocks until Armed or execution is stopping, stopped or failed.
					 */
					void WaitArmed() noexcept;

					/**
					 * @brief Creates the container backend from the Plan extension.
					 * @return false after Fail.
					 */
					bool SpawnBackend() noexcept;

					/**
					 * @brief Opens the Plan writer and binds FileAvio once.
					 * @return false after Fail.
					 *
					 * Idempotent after the backend context exists.
					 */
					bool ArmOctets() noexcept;

					/**
					 * @brief Copies attachments from the Plan snapshot.
					 * @return false after Fail.
					 *
					 * Uses @ref Plan::Snapshot. Does not Open the reader again.
					 * Empty catalogue if the Plan has no attachment tracks.
					 */
					bool BindPlanAttachments() noexcept;

					/**
					 * @brief Flushes the Plan writer after the trailer.
					 *
					 * Called when the mux worker finishes the trailer.
					 */
					void FlushOctets() noexcept;

					/**
					 * @brief Copies an opened encoder into a libav output stream.
					 * @param encoder Reserved encode lane.
					 * @param avStream libav AVStream*.
					 * @return false if the encoder has no context.
					 */
					bool BindEncoderStream(Encoder& encoder, void* avStream) noexcept;

					/**
					 * @brief Clones remux codecpar from the bound demuxer.
					 * @param inIndex Origin stream index.
					 * @param params Owned AVCodecParameters* on success.
					 * @param timeBase AVRational*.
					 * @return false if the origin is not ready.
					 */
					bool RemuxCodec(int inIndex, void*& params, void* timeBase) noexcept;

					/**
					 * @brief Video / Audio / Subtitle entries in the bound Plan.
					 * @return 0 when there is no Plan.
					 */
					StormByte::Size ExpectedSlots() const noexcept;

					/**
					 * @brief Marks the shared clock MuxDone. Friend: mux worker Flush.
					 */
					void ClockMuxDone() noexcept;

					/**
					 * @brief Advances All from a written packet. Friend: mux worker Process.
					 * @param ns Presentation time written, nanoseconds.
					 */
					void ClockPass(std::int64_t ns) noexcept;

					const Container* m_container;					///< Destination container (Plan)
					StormByte::Safe::Unique<Backend::Pipeline::Muxer> m_backend;	///< Format backend
					Demuxer* m_origin;						///< Set only by demuxer >> muxer. Not owned
					StormByte::Safe::Shared<class Progress> m_progress;		///< Shared tube clock retained through Base-heap ownership.
					Attachments m_attachments;					///< Catalogue for header write
					StormByte::Safe::Set<int> m_wired;					///< Remux input indices already reserved; encoder output indices are checked by the backend.
					StormByte::Safe::Map<int, StormByte::Safe::String> m_language;	///< Per-output language
					StormByte::Safe::Map<int, StormByte::Safe::String> m_title;		///< Per-output title
					StormByte::Safe::Atomic<bool> m_closed;				///< Set by Finish / Fail
					StormByte::Safe::Atomic<std::uint64_t> m_reserved;			///< Reserved Video/Audio/Subtitle hoppers
					StormByte::Safe::Atomic<std::int64_t> m_positionNs;			///< Last written Pts, or -1
					Join m_join{*this};						///< Halt before other members die
			};
		}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Muxer);

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

#include <StormByte/bitmask.hxx>
#include <StormByte/buffer/hopper.hxx>
#include <StormByte/expected.hxx>
#include <StormByte/multimedia/pipeline/exception.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/type_traits.hxx>

#include <cstdint>
#include <memory>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte suite.
 * @ingroup multimedia_pipeline
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Buffer
	 * @brief Typed item queues provided by StormByte-Buffer.
	 */
	namespace Buffer {
		/**
		 * @brief Forward declaration using Buffer's typed queue contract.
		 * @tparam T Nonthrowing Safe value supported by Hopper.
		 */
		template<Detail::HopperValue T>
		class Sink;
	}

	/**
	 * @namespace StormByte::Multimedia
	 * @brief Audio and video processing types.
	 * @ingroup multimedia_pipeline
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Pipeline
		 * @brief Demux / decode / filter / encode / mux types.
		 * @ingroup multimedia_pipeline
		 */
		namespace Pipeline {
			/**
			 * @brief Outcome of @ref Plan::Check.
			 */
			using CheckResult = StormByte::Expected<void, PlanException>;

			/**
			 * @enum Kind
			 * @brief Whether an @ref Item is a decoded frame or a compressed packet.
			 *
			 * Distinct from @ref StormByte::Multimedia::Type (Video / Audio / Subtitle).
			 * Values are bit flags. @c 0 is not a valid kind. Combine with @ref Kinds.
			 * @ref ToString understands a single flag only.
			 *
			 * @ingroup multimedia_pipeline
			 */
			enum class Kind: std::uint8_t {
				Packet = 1 << 0,		///< Compressed @ref StormByte::Multimedia::Pipeline::Packet.
				Frame  = 1 << 1			///< Decoded @ref StormByte::Multimedia::Pipeline::Frame.
			};

			/**
			 * @brief Converts a single @ref Kind flag to a string literal.
			 * @param kind Value to convert.
			 * @return `"Frame"`, `"Packet"`, or `"Invalid"` for a mask or zero.
			 */
			constexpr const char* ToString(Kind kind) noexcept {
				switch (kind) {
					case Kind::Frame:	return "Frame";
					case Kind::Packet:	return "Packet";
					default:			return "Invalid";
				}
			}

			/**
			 * @enum Producer
			 * @brief Named stage of the tube.
			 *
			 * Used as @ref Item origin and as @ref Step display name for logs.
			 * @c Filter covers @ref Filter::FFmpeg and its Process / Packet / Analytics leaves.
			 *
			 * @ingroup multimedia_pipeline
			 */
			enum class Producer: std::uint8_t {
				Demuxer,		///< @ref Demuxer stage.
				Decoder,		///< @ref Decoder stage.
				Remuxer,		///< @ref Remuxer stage.
				Filter,			///< @ref Filter::FFmpeg stage.
				Route,			///< A configured connection between pipeline stages.
				Filters,		///< @ref Filters stage.
				Encoder,		///< @ref Encoder stage.
				Muxer			///< @ref Muxer stage.
			};

			/**
			 * @brief Converts a @ref Producer to a string literal.
			 * @param producer Value to convert.
			 * @return Null-terminated name, or `"Invalid"`.
			 */
			constexpr const char* ToString(Producer producer) noexcept {
				switch (producer) {
					case Producer::Demuxer:	return "Demuxer";
					case Producer::Decoder:	return "Decoder";
					case Producer::Remuxer:	return "Remuxer";
					case Producer::Filter:	return "Filter";
					case Producer::Route:	return "Route";
					case Producer::Filters:	return "Filters";
					case Producer::Encoder:	return "Encoder";
					case Producer::Muxer:	return "Muxer";
					default:				return "Invalid";
				}
			}

			/**
			 * @class Kinds
			 * @brief Bitmask of @ref Kind.
			 *
			 * Every @ref Step stores one mask as Receives and one as Produces.
			 * Empty means that side does not take or emit items (Demux receives
			 * nothing, Mux produces nothing).
			 *
			 * Tests use @ref StormByte::Bitmask::Has (all bits) and
			 * @ref StormByte::Bitmask::HasAny.
			 *
			 * @ingroup multimedia_pipeline
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Kinds: public StormByte::Bitmask<Kinds, Kind> {
				public:
					/**
					 * @brief Inherits the bitmask constructors.
					 */
					using StormByte::Bitmask<Kinds, Kind>::Bitmask;
			};

			/**
			 * @enum State
			 * @brief Lifecycle of the stage thread. Values are mutually exclusive.
			 *
			 * Describes an individual stage, not the whole job or its neighbors.
			 * Inspect it through @ref Step::Status.
			 *
			 * - Created: constructed and available for configuration, but not ready
			 *   to process media. Binding a Plan, connecting stages, stopping and
			 *   reporting failure are permitted.
			 * - Ready: initialized and processing media. End of input leads to
			 *   Stopped; cancellation or failure may also end processing.
			 * - Stopping: shutdown has been requested but has not completed.
			 *   This is not a failure; pending output may still be flushed.
			 * - Stopped: processing has ended, naturally or after a stop request.
			 * - Failed: processing failed; @ref Step::Error provides the reason.
			 *
			 * Legal transitions are Created to Ready, Failed or Stopping;
			 * Ready to Stopping, Failed or Stopped; and Stopping to Stopped.
			 * Failed and Stopped are terminal: stages cannot be restarted.
			 * Source end-of-file is reported separately by @ref Demuxer::Eof.
			 *
			 * @ingroup multimedia_pipeline
			 */
			enum class State: std::uint8_t {
				Created,		///< Constructed and available for configuration.
				Ready,			///< Initialized and processing media.
				Stopping,		///< Shutdown requested but not completed.
				Stopped,		///< Processing has ended.
				Failed			///< Processing failed; @ref Step::Error provides the reason.
			};
		}
	}
}

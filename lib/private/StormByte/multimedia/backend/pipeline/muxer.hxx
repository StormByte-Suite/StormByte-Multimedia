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

#include <StormByte/multimedia/attachment.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/visibility.h>

#include <memory>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	namespace Multimedia {
		namespace Backend {
			namespace Pipeline {
				/**
				 * @class Muxer
				 * @brief Mux backend behind one public Muxer.
				 *
					 * A shared libavformat leaf selects separate format policies. BindSink allocates
				 * the format context and a write @ref FileAvio on
				 * @ref Plan::Writer. Libav must not open a path.
				 * ReserveEncoder, ReserveRemux and BindAttachments stay
				 * bool+Fail. Push uses shared_ptr. Errors are owner.Fail().
				 *
				 * @ingroup multimedia_pipeline
				 */
				class STORMBYTE_MULTIMEDIA_PRIVATE Muxer {
					public:
						/**
						 * @brief Destructor.
						 */
						virtual ~Muxer() noexcept = default;

						/**
						 * @brief Copy constructor (deleted).
						 */
						Muxer(const Muxer& other) = delete;

						/**
						 * @brief Copy assignment (deleted).
						 * @return *this.
						 */
						Muxer& operator=(const Muxer& other) = delete;

						/**
						 * @brief Whether the output context exists.
						 * @return true after a successful @ref BindSink.
						 */
						virtual bool IsOpen() const noexcept = 0;

						/**
						 * @brief Whether avformat_write_header has run.
						 * @return true after a successful header.
						 */
						virtual bool HeaderWritten() const noexcept = 0;

						/**
						 * @brief Allocates the format context and write AVIO.
						 * @param owner Public muxer (Fail, Destination, Plan writer).
						 * @return false if owner.Fail() was called.
						 *
						 * Format is guessed from the actual Plan writer filename.
						 * Bytes go to @ref Plan::Writer through FileAvio.
						 */
						virtual bool BindSink(StormByte::Multimedia::Pipeline::Muxer& owner) noexcept = 0;

						/**
						 * @brief Reserves an encode track at Encoder::Index().
						 * @param owner Public muxer.
						 * @param encoder Live encoder (must outlive the header).
						 * @return false if owner.Fail() was called.
						 */
						virtual bool ReserveEncoder(StormByte::Multimedia::Pipeline::Muxer& owner,
							StormByte::Multimedia::Pipeline::Encoder& encoder) noexcept = 0;

						/**
						 * @brief Reserves a remux slot for origin stream @p inIndex.
						 * @param owner Public muxer.
						 * @param inIndex Origin stream index.
						 * @return false if owner.Fail() was called.
						 */
						virtual bool ReserveRemux(StormByte::Multimedia::Pipeline::Muxer& owner,
							int inIndex) noexcept = 0;

						/**
						 * @brief Binds a collected attachment catalogue.
						 * @param owner Public muxer.
						 * @param attachments Catalogue. Not a File.
						 * @return false if owner.Fail() was called.
						 */
						virtual bool BindAttachments(StormByte::Multimedia::Pipeline::Muxer& owner,
							const StormByte::Multimedia::Attachments& attachments) noexcept = 0;

						/**
						 * @brief Queues or writes one packet.
						 * @param owner Public muxer.
						 * @param packet Encoded or remuxed packet.
						 * @return true if the packet was accepted.
						 */
						virtual bool Push(StormByte::Multimedia::Pipeline::Muxer& owner,
							const StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet>& packet) noexcept = 0;

						/**
						 * @brief Flushes leftover packets and the trailer.
						 * @param owner Public muxer.
						 */
						virtual void Flush(StormByte::Multimedia::Pipeline::Muxer& owner) noexcept = 0;

						/**
						 * @brief Writes trailer if needed and frees AVIO + context.
						 */
						virtual void Close() noexcept = 0;

					protected:
						/**
						 * @brief Default constructor.
						 */
						Muxer() noexcept = default;

						/**
						 * @brief Move constructor.
						 * @param other Backend to take.
						 */
						Muxer(Muxer&& other) noexcept = default;

						/**
						 * @brief Move assignment.
						 * @param other Backend to take.
						 * @return *this.
						 */
						Muxer& operator=(Muxer&& other) noexcept = default;

						/**
						 * @brief Asks the public Muxer to copy @p encoder into @p avStream.
						 * @param owner Public muxer.
						 * @param encoder Live encoder.
						 * @param avStream libav AVStream*.
						 * @return false if the encoder has no context.
						 */
						bool BindEncoderStream(StormByte::Multimedia::Pipeline::Muxer& owner,
							StormByte::Multimedia::Pipeline::Encoder& encoder, void* avStream) noexcept {
							return owner.BindEncoderStream(encoder, avStream);
						}
				};
			}
		}
	}
}

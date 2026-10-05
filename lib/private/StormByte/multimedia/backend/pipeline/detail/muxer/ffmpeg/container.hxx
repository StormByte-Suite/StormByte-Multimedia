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

#include <StormByte/multimedia/attachment.hxx>
#include <StormByte/multimedia/backend/file_avio.hxx>
#include <StormByte/multimedia/backend/pipeline/muxer.hxx>
#include <StormByte/multimedia/ffmpeg/AVRational.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg
 * @brief Generic libavformat output mux backend.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg {
	using StormByte::Multimedia::FFmpeg::AVRational;

	/**
	 * @class Container
	 * @brief libavformat mux backend for registered destination containers.
	 *
	 * Owns the output AVFormatContext, reserved tracks and the
	 * header-delay queue. Bytes leave through @ref Backend::FileAvio
	 * on @ref Plan::Writer. Format is guessed from the registry container
	 * extension. Matroska attachments and header options remain conditional
	 * on destination capabilities.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Container final: public StormByte::Multimedia::Backend::Pipeline::Muxer {
		public:
			/**
			 * @brief Empty output context.
			 */
			Container() noexcept;

			/**
			 * @brief Writes trailer if needed and frees the context.
			 */
			~Container() noexcept override;

			/**
			 * @brief Copy constructor (deleted).
			 */
			Container(const Container& other) = delete;

			/**
			 * @brief Copy assignment (deleted).
			 * @return *this.
			 */
			Container& operator=(const Container& other) = delete;

			/**
			 * @brief Move constructor (deleted).
			 */
			Container(Container&& other) noexcept = delete;

			/**
			 * @brief Move assignment (deleted).
			 * @return *this.
			 */
			Container& operator=(Container&& other) noexcept = delete;

			/**
			 * @brief Whether the output context exists.
			 * @return true after BindSink().
			 */
			bool IsOpen() const noexcept override;

			/**
			 * @brief Whether avformat_write_header has run.
			 * @return true after a successful header.
			 */
			bool HeaderWritten() const noexcept override;

			/**
			 * @brief Allocates the format context and write AVIO.
			 * @param owner Public muxer.
			 * @return false if owner.Fail() was called.
			 *
			 * Format comes from @c owner.Destination().Extension().
			 * Bytes go to @ref Plan::Writer via FileAvio. No path.
			 */
			bool BindSink(StormByte::Multimedia::Pipeline::Muxer& owner) noexcept override;

			/**
			 * @brief Reserves an encode track.
			 * @param owner Public muxer.
			 * @param encoder Live encoder.
			 * @return false if owner.Fail() was called.
			 */
			bool ReserveEncoder(StormByte::Multimedia::Pipeline::Muxer& owner,
				StormByte::Multimedia::Pipeline::Encoder& encoder) noexcept override;

			/**
			 * @brief Reserves a remux output slot for origin stream @p inIndex.
			 * @param owner Public muxer.
			 * @param inIndex Origin stream index.
			 * @return false if owner.Fail() was called.
			 */
			bool ReserveRemux(StormByte::Multimedia::Pipeline::Muxer& owner,
				int inIndex) noexcept override;

			/**
			 * @brief Binds a collected attachment catalogue for header time.
			 * @param owner Public muxer.
			 * @param attachments Catalogue. Not a File.
			 * @return false if owner.Fail() was called.
			 */
			bool BindAttachments(StormByte::Multimedia::Pipeline::Muxer& owner,
				const StormByte::Multimedia::Attachments& attachments) noexcept override;

			/**
			 * @brief Queues or writes one packet.
			 * @param owner Public muxer.
			 * @param packet Encoded or remuxed packet.
			 * @return true if the packet was accepted.
			 */
			bool Push(StormByte::Multimedia::Pipeline::Muxer& owner,
				const StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet>& packet) noexcept override;

			/**
			 * @brief Writes leftover packets and the trailer.
			 * @param owner Public muxer.
			 */
			void Flush(StormByte::Multimedia::Pipeline::Muxer& owner) noexcept override;

			/**
			 * @brief Writes trailer if needed and frees AVIO + context.
			 */
			void Close() noexcept override;

		private:
			/**
			 * @class Track
			 * @brief Reserved output slot for an encoder or a remux track.
			 */
			class Track {
				public:
					StormByte::Multimedia::Pipeline::Encoder* encoder = nullptr;	///< Live encoder. Null on remux
					int inIndex = -1;												///< Source stream index if remux
					::AVCodecParameters* params = nullptr;							///< Cloned remux codecpar
					AVRational srcTb{0, 1};											///< Source time base if remux
					int avIndex = -1;												///< Index in AVFormatContext
					AVRational timeBase{0, 1};										///< Mux time base
					std::int64_t lastDts = StormByte::Multimedia::FFmpeg::NoPts;	///< Last written DTS
					StormByte::Safe::Optional<StormByte::Safe::String> language;	///< Header language
					StormByte::Safe::Optional<StormByte::Safe::String> title;		///< Header title
			};

			/**
			 * @brief Maps a packet track onto an output slot.
			 * @param track Packet::Track().
			 * @return Output key, or -1.
			 */
			int Resolve(int track) const noexcept;

			/**
			 * @brief Writes the header when Armed and every reserved encoder is open.
			 * @param owner Public muxer.
			 * @return false if owner.Fail() was called.
			 */
			bool WriteHeaderIfReady(StormByte::Multimedia::Pipeline::Muxer& owner) noexcept;

			/**
			 * @brief Writes one packet after the header.
			 * @param owner Public muxer.
			 * @param packet Source packet.
			 * @return false if owner.Fail() was called.
			 */
			bool WritePacket(StormByte::Multimedia::Pipeline::Muxer& owner,
				StormByte::Multimedia::Pipeline::Packet& packet) noexcept;

			/**
			 * @brief Frees cloned remux codecpar.
			 */
			void FreeParams() noexcept;

			::AVFormatContext* m_ctx;									///< Output format context
			std::optional<StormByte::Multimedia::Backend::FileAvio> m_avio;	///< Writer AVIO
			std::map<int, Track> m_tracks;								///< Output index → track
			std::map<int, int> m_inToOut;								///< Source index → output index
			std::deque<StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet>> m_queue;	///< Packets waiting for header
			StormByte::Multimedia::Attachments m_attachments;			///< Header catalogue
			bool m_header;												///< avformat_write_header done
			bool m_trailer;												///< av_write_trailer done
			bool m_customIo;											///< pb is FileAvio
	};
}

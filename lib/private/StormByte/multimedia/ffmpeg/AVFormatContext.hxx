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

#include <StormByte/buffer/consumer.hxx>
#include <StormByte/multimedia/ffmpeg/AVPointer.hxx>
#include <StormByte/multimedia/ffmpeg/AVBSF.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/ffmpeg/backend_typedefs.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

#include <chrono>

namespace StormByte::Multimedia {
	class File;
}

namespace StormByte::Multimedia::Backend::Pipeline {
	class Demuxer;
}

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	class AVBSF;
	class AVCodecParameters;
	class AVPacket;
	class AVStream;

	/**
	 * @class AVFormatContext
	 * @brief RAII input format context (demuxer).
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE AVFormatContext: public AVPointer<::AVFormatContext> {
		friend class StormByte::Multimedia::File;
		friend class StormByte::Multimedia::Backend::Pipeline::Demuxer;

		public:
			/**
			 * @brief Copy constructor (deleted).
			 * @param other Unused.
			 */
			AVFormatContext(const AVFormatContext& other) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Source context.
			 */
			AVFormatContext(AVFormatContext&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			~AVFormatContext() noexcept override;

			/**
			 * @brief Copy assignment (deleted).
			 * @param other Unused.
			 * @return *this.
			 */
			AVFormatContext& operator=(const AVFormatContext& other) = delete;

			/**
			 * @brief Move assignment.
			 * @param other Source context.
			 * @return *this.
			 */
			AVFormatContext& operator=(AVFormatContext&& other) noexcept;

			/**
			 * @brief Opens a file and finds stream info.
			 * @param path Media path.
			 * @return Context or DecoderError.
			 */
			static ExpectedAVFormatContext Open(const Safe::String& path);

			/**
			 * @brief Opens a Consumer and finds stream info.
			 * @param consumer Shared ring handle (copied into the I/O adapter).
			 * @return Context or DecoderError.
			 */
			static ExpectedAVFormatContext Open(StormByte::Buffer::Consumer consumer);

			/**
			 * @brief Demuxer format name (`iformat->name`).
			 * @return Owned name (may be comma-separated ids), or empty.
			 */
			Safe::String FormatName() const noexcept;

			/**
			 * @brief Looks up a container metadata tag.
			 * @param key Dictionary key (e.g. `"title"`).
			 * @return Owned value, or empty if missing; independent of the context lifetime.
			 */
			Safe::Optional<Safe::String> Tag(const char* key) const noexcept;

			/**
			 * @brief Container duration in nanoseconds.
			 * @return Duration, or empty if unknown or result allocation fails.
			 */
			Safe::Optional<Property::Duration> Duration() const;

			/**
			 * @brief Reads the next packet.
			 * @param packet Destination packet.
			 * @return Operation result.
			 */
			OperationResult ReadPacket(AVPacket& packet) noexcept;

			/**
			 * @brief Non-owning stream views.
			 * @return Views ordered by stream index; this context must outlive all views.
			 */
			Streams Streams() const noexcept;

			/**
			 * @brief Returns an mp4→Annex-B BSF when the container and codec require it.
			 * @param codec_id Codec id.
			 * @param stream_id Stream index (time base).
			 * @param params Codec parameters.
			 * @return Base-heap filter owner, or empty if unnecessary or creation failed.
			 */
			Safe::Unique<AVBSF> Mp4ToAnnexB(int codec_id, int stream_id, const AVCodecParameters& params) const noexcept;

			/**
			 * @brief Whether an input context is open.
			 * @return true after a successful Open().
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief Marks streams not in @p wanted as `AVDISCARD_ALL`.
			 * @param wanted Origin indexes to keep (`AVDISCARD_DEFAULT`).
			 * @note Looks up indexes directly without allocating an intermediate collection.
			 */
			void DiscardUnwanted(const Safe::Vector<int>& wanted) noexcept;

		private:
			struct ConsumerIO;

			Safe::Unique<ConsumerIO> m_io;	///< Base-owned custom AVIO state (Consumer opens).
			bool m_avioBorrowed;				///< true: @c pb is not owned

			/**
			 * @brief Adopts a raw format context.
			 * @param ctx Raw format context (owned).
			 * @param io Optional Consumer AVIO state.
			 * @param avioBorrowed true if @c pb must outlive this context.
			 */
			explicit AVFormatContext(::AVFormatContext* ctx, Safe::Unique<ConsumerIO> io,
				bool avioBorrowed = false) noexcept;

			/**
			 * @brief Adopts a raw context whose @c pb is owned elsewhere.
			 * @param ctx Raw format context (owned).
			 * @return Wrapper. Detaches @c pb on Free.
			 *
			 * Defined in this TU so ConsumerIO stays incomplete at the call site.
			 */
			static AVFormatContext WrapBorrowed(::AVFormatContext* ctx) noexcept;

			/**
			 * @brief Wrap custom AVIO with optional HDR side-data collection.
			 * @param ctx Input format context to own without owning its AVIO.
			 * @param harvest true to decode for missing side data; false for timestamp scans or restored metadata.
			 * @return Owned format wrapper.
			 */
			static AVFormatContext WrapBorrowed(::AVFormatContext* ctx, bool harvest) noexcept;

			/**
			 * @brief Copies HDR side data from early decoded frames onto codecpar.
			 */
			void HarvestSideData() noexcept;

			/**
			 * @brief Seeks the origin to timestamp zero without closing the context.
			 * @return Operation result.
			 *
			 * Friend: Backend::Pipeline::Demuxer::Rewind. Used after a
			 * ProcessTwoPasses measure pass. Also rewinds Consumer I/O.
			 */
			OperationResult SeekStart() noexcept;

			/**
			 * @brief Closes input. Borrowed AVIO is detached, not freed.
			 */
			void Free() noexcept override;

			using AVPointer<::AVFormatContext>::Get;
	};

	extern template class STORMBYTE_MULTIMEDIA_PRIVATE AVPointer<::AVFormatContext>;
}

/**
 * @brief Conditional provider contract: context and private AVIO lifetimes stay in Multimedia.
 * @note Borrowed AVIO and stream views retain their documented owner lifetimes.
 * Multimedia, Base and FFmpeg must remain loaded with compatible ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVFormatContext);

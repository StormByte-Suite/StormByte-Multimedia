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
#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstdint>
#include <string_view>

/**
 * @struct AVDictionary
 * @brief Opaque libavutil dictionary used for output header options.
 */
struct AVDictionary;

/**
 * @namespace StormByte::Multimedia::Pipeline
 * @brief Demux / decode / filter / encode / mux types.
 */
namespace StormByte::Multimedia::Pipeline {
	/**
	 * @class Muxer
	 * @brief Public packet-writing stage supplied to private format policies.
	 */
	class Muxer;
}

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg
 * @brief Generic libavformat output mux backend.
 */
namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg {
	/**
	 * @class Policy
	 * @brief Stateless container adaptations selected from the resolved FFmpeg muxer.
	 *
	 * The default policy leaves stream compatibility and output shape to libavformat.
	 * Policies never own the writer, format context, packets or reserved tracks.
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Policy {
		public:
			/**
			 * @brief Constructs the unrestricted default policy.
			 */
			Policy() noexcept;

			/**
			 * @brief Copy construction is disabled.
			 * @param other Source policy.
			 */
			Policy(const Policy& other) = delete;

			/**
			 * @brief Move construction is disabled.
			 * @param other Source policy.
			 */
			Policy(Policy&& other) = delete;

			/**
			 * @brief Destroys the stateless policy.
			 */
			virtual ~Policy() noexcept;

			/**
			 * @brief Copy assignment is disabled.
			 * @param other Source policy.
			 * @return This policy.
			 */
			Policy& operator=(const Policy& other) = delete;

			/**
			 * @brief Move assignment is disabled.
			 * @param other Source policy.
			 * @return This policy.
			 */
			Policy& operator=(Policy&& other) = delete;

			/**
			 * @brief Applies container-specific stream defaults; the base does nothing.
			 * @param stream Reserved FFmpeg output stream.
			 * @param haveDefaultVideo Whether a video default was already selected.
			 * @param haveDefaultAudio Whether an audio default was already selected.
			 */
			virtual void PrepareStream(::AVStream& stream, bool& haveDefaultVideo,
				bool& haveDefaultAudio) const noexcept;

			/**
			 * @brief Adds attachment streams, leaving format acceptance to libavformat.
			 * @param owner Public muxer receiving failures.
			 * @param context Output context receiving attachment streams.
			 * @param attachments Requested attachment catalogue.
			 * @return True on success, false if the owner failed.
			 */
			virtual bool WriteAttachments(StormByte::Multimedia::Pipeline::Muxer& owner,
				::AVFormatContext& context, const StormByte::Multimedia::Attachments& attachments) const noexcept;

			/**
			 * @brief Emits static resources once after libavformat writes the header.
			 * @param owner Public muxer receiving failures.
			 * @param context Output context with initialized stream time bases.
			 * @return True on success, false if a resource packet could not be written.
			 */
			virtual bool WriteHeaderPackets(StormByte::Multimedia::Pipeline::Muxer& owner,
				::AVFormatContext& context) const noexcept;

			/**
			 * @brief Applies header adaptations; the default preserves FFmpeg settings.
			 * @param owner Public muxer with the bound plan.
			 * @param context Output context to configure.
			 * @param bufferedSpanUs Timestamp span of packets waiting for the header.
			 * @param options FFmpeg header dictionary to populate.
			 */
			virtual void ConfigureHeader(const StormByte::Multimedia::Pipeline::Muxer& owner,
				::AVFormatContext& context, std::int64_t bufferedSpanUs, ::AVDictionary** options) const noexcept;

		protected:
			/**
			 * @brief Marks the first video and audio streams as defaults.
			 * @param stream Output stream to inspect.
			 * @param haveDefaultVideo Whether a video default was already selected.
			 * @param haveDefaultAudio Whether an audio default was already selected.
			 */
			static void MarkDefaultStream(::AVStream& stream, bool& haveDefaultVideo,
				bool& haveDefaultAudio) noexcept;
	};

	/**
	 * @brief Selects exceptional policies; every other resolved muxer uses the default.
	 * @param formatName Name reported by the actual FFmpeg output format.
	 * @return Process-lifetime immutable policy.
	 */
	const Policy& SelectPolicy(std::string_view formatName) noexcept;

	/**
	 * @brief Returns the unrestricted libavformat policy.
	 * @return Process-lifetime immutable policy.
	 */
	const Policy& GenericPolicy() noexcept;

	/**
	 * @brief Returns Matroska attachment and header adaptations.
	 * @return Process-lifetime immutable policy.
	 */
	const Policy& MatroskaPolicy() noexcept;

	/**
	 * @brief Returns WebM-specific adaptations.
	 * @return Process-lifetime immutable policy.
	 */
	const Policy& WebmPolicy() noexcept;

	/**
	 * @brief Returns MOV/MP4 stream adaptations.
	 * @return Process-lifetime immutable policy.
	 */
	const Policy& Mp4Policy() noexcept;
}

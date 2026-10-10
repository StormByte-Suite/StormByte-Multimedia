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

#include <StormByte/multimedia/ffmpeg/AVCodecParameters.hxx>
#include <StormByte/multimedia/ffmpeg/AVEncoder.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/features.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

extern "C" {
	struct AVCodecContext;
}

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	using FFmpeg::AVRational;
	/**
	 * @class Encoder
	 * @brief Encode backend behind one public Encoder.
	 *
	 * Push / Take use shared_ptr. Errors are owner.Fail.
	 * Take empty means no packet ready (or drain finished after Flush).
	 * Table lookup and avcodec_open2 live here. Media-specific
	 * parameters are filled by Detail::Encoder leaves.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Encoder {
		public:
			/**
			 * @class Opened
			 * @brief Result of a successful table lookup and avcodec_open2.
			 */
			class STORMBYTE_MULTIMEDIA_PRIVATE Opened {
				public:
					/**
					 * @brief Empty construction is disabled; an opened encoder is required.
					 */
					Opened() noexcept = delete;

					/**
					 * @brief Takes ownership of an opened encoder.
					 * @param encoder Opened FFmpeg encoder.
					 * @param timeBase Encoder time base.
					 * @param implementation Selected row name.
					 * @param capabilities Selected row features.
					 */
					Opened(StormByte::Multimedia::FFmpeg::AVEncoder encoder,
						AVRational timeBase,
						std::string implementation,
						StormByte::Multimedia::Features capabilities) noexcept
					: m_encoder(std::move(encoder)),
					m_timeBase(timeBase),
					m_implementation(std::move(implementation)),
					m_capabilities(capabilities) {}

					/**
					 * @brief Opened FFmpeg encoder.
					 * @return Encoder.
					 */
					inline StormByte::Multimedia::FFmpeg::AVEncoder& Handle() noexcept {
						return m_encoder;
					}

					/**
					 * @brief Opened FFmpeg encoder.
					 * @return Encoder.
					 */
					inline const StormByte::Multimedia::FFmpeg::AVEncoder& Handle() const noexcept {
						return m_encoder;
					}

					/**
					 * @brief Encoder time base.
					 * @return Time base.
					 */
					inline AVRational TimeBase() const noexcept {
						return m_timeBase;
					}

					/**
					 * @brief Sets the encoder time base.
					 * @param timeBase Time base.
					 */
					inline void TimeBase(AVRational timeBase) noexcept {
						m_timeBase = timeBase;
					}

					/**
					 * @brief Selected table row name.
					 * @return Name.
					 */
					inline const std::string& Implementation() const noexcept {
						return m_implementation;
					}

					/**
					 * @brief Sets the selected table row name.
					 * @param implementation Row name.
					 */
					inline void Implementation(std::string implementation) noexcept {
						m_implementation = std::move(implementation);
					}

					/**
					 * @brief Selected row features.
					 * @return Mask.
					 */
					inline const StormByte::Multimedia::Features& Capabilities() const noexcept {
						return m_capabilities;
					}

					/**
					 * @brief Sets the selected row features.
					 * @param capabilities Mask.
					 */
					inline void Capabilities(StormByte::Multimedia::Features capabilities) noexcept {
						m_capabilities = capabilities;
					}

				private:
					StormByte::Multimedia::FFmpeg::AVEncoder m_encoder;	///< Opened encoder
					AVRational m_timeBase{};										///< Encoder time base
					std::string m_implementation;									///< Selected row name
					StormByte::Multimedia::Features m_capabilities;				///< Selected row features
			};

			/**
			 * @brief Destructor.
			 */
			virtual ~Encoder() noexcept = default;

			/**
			 * @brief Copy constructor.
			 * @param other Source backend.
			 */
			Encoder(const Encoder& other) = delete;

			/**
			 * @brief Copy assignment.
			 * @param other Source backend.
			 * @return *this.
			 */
			Encoder& operator=(const Encoder& other) = delete;

			/**
			 * @brief Whether the FFmpeg encoder is open.
			 * @return true after a successful Open().
			 */
			virtual bool IsOpen() const noexcept = 0;

			/**
			 * @brief Prepares codecs whose headers do not require a first frame.
			 * @param owner Public encoder with completed output configuration.
			 * @return True when ready or when opening must remain frame-driven.
			 */
			virtual bool PrepareForHeader(StormByte::Multimedia::Pipeline::Encoder& owner) noexcept;

			/**
			 * @brief Requests header preparation through the public encoder's private backend.
			 * @param owner Public encoder being reserved by the muxer.
			 * @return False if preparation failed or the backend is absent.
			 */
			static bool PrepareForMux(StormByte::Multimedia::Pipeline::Encoder& owner) noexcept;

			/**
			 * @brief Picks the table row and opens the FFmpeg encoder from leaf parameters.
			 * @param owner Public encoder.
			 * @param frame First frame (leaf may inspect it).
			 * @return false if owner.Fail() was called.
			 */
			virtual bool Open(StormByte::Multimedia::Pipeline::Encoder& owner,
				const StormByte::Multimedia::Pipeline::Frame& frame) noexcept = 0;

			/**
			 * @brief Encodes one frame. Opens lazily on first call if needed.
			 * @param owner Public encoder.
			 * @param frame Decoded frame.
			 * @return true if libav accepted it.
			 */
			virtual bool Push(StormByte::Multimedia::Pipeline::Encoder& owner,
				const StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Frame>& frame) noexcept = 0;

			/**
			 * @brief Signals EOF and drains. No-op if already flushed.
			 * @param owner Public encoder.
			 */
			virtual void Flush(StormByte::Multimedia::Pipeline::Encoder& owner) noexcept = 0;

			/**
			 * @brief Pops one pending encoded packet.
			 * @return Packet with Producer::Encoder, or empty if none ready.
			 */
			virtual StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet> Take() noexcept = 0;

			/**
			 * @brief Opened AVCodecContext, if any.
			 * @return Context, or nullptr before Open().
			 */
			virtual const AVCodecContext* Context() const noexcept = 0;

			/**
			 * @brief Encoder time base after Open().
			 * @return Rational. {0,1} before Open().
			 */
			virtual AVRational TimeBase() const noexcept = 0;

			/**
			 * @brief Picks the table row, applies owner setters and opens FFmpeg.
			 * @param owner Public encoder (Fail, CRF, FineTune, Destination).
			 * @param params Codec parameters filled by the media leaf.
			 * @param timeBase Encoder time base chosen by the media leaf.
			 * @param need Extra feature bits the leaf derived from the frame.
			 * @param firstFrame Optional decoded metadata used to configure native video encoding.
			 * @return Opened backend, or empty after owner.Fail().
			 */
			static std::optional<Opened> OpenCodec(StormByte::Multimedia::Pipeline::Encoder& owner,
				StormByte::Multimedia::FFmpeg::AVCodecParameters params,
				AVRational timeBase,
				StormByte::Multimedia::Features need,
				const StormByte::Multimedia::FFmpeg::AVFrame* firstFrame = nullptr) noexcept;

			/**
			 * @brief Builds a pipeline Packet from an encoded AVPacket via owner.Wrap.
			 * @param owner Public encoder.
			 * @param type Kind of the encoded access unit (destination codec Type).
			 * @param index Mux output index.
			 * @param raw Encoder output.
			 * @param timeBase Encoder time base.
			 * @param keepPacketHdrPlus false for HEVC (SEI already in payload).
			 * @return shared Packet with Producer::Encoder.
			 */
			static StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet> MakePacket(
				StormByte::Multimedia::Pipeline::Encoder& owner,
				enum StormByte::Multimedia::Type type, int index,
				const StormByte::Multimedia::FFmpeg::AVPacket& raw,
				AVRational timeBase, bool keepPacketHdrPlus = true) noexcept;

			/**
			 * @brief Converts nanoseconds to encoder ticks.
			 * @param ns Nanoseconds.
			 * @param timeBase Encoder time base.
			 * @return Ticks, or AV_NOPTS_VALUE.
			 */
			static std::int64_t NsToTicks(std::int64_t ns, AVRational timeBase) noexcept;

		protected:
			/**
			 * @brief Default constructor.
			 */
			Encoder() noexcept = default;

			/**
			 * @brief Move constructor.
			 * @param other Backend to take.
			 */
			Encoder(Encoder&& other) noexcept = default;

			/**
			 * @brief Move assignment.
			 * @param other Backend to take.
			 * @return *this.
			 */
			Encoder& operator=(Encoder&& other) noexcept = default;

			/**
			 * @brief FFmpeg frame handle bound to @p frame, if any.
			 * @param owner Public encoder.
			 * @param frame Public unit.
			 * @return Handle, or nullptr.
			 */
			static StormByte::Multimedia::FFmpeg::AVFrame* FrameHandle(
				StormByte::Multimedia::Pipeline::Encoder& owner,
				StormByte::Multimedia::Pipeline::Frame& frame) noexcept;

			/**
			 * @brief FFmpeg frame handle bound to @p frame, if any.
			 * @param owner Public encoder.
			 * @param frame Public unit.
			 * @return Handle, or nullptr.
			 */
			static const StormByte::Multimedia::FFmpeg::AVFrame* FrameHandle(
				StormByte::Multimedia::Pipeline::Encoder& owner,
				const StormByte::Multimedia::Pipeline::Frame& frame) noexcept;

			/**
			 * @brief Copies implementation name and capabilities onto @p owner.
			 * @param owner Public encoder.
			 * @param opened Successful OpenCodec result.
			 */
			static void CommitOpen(StormByte::Multimedia::Pipeline::Encoder& owner,
				const Opened& opened) noexcept;
	};
}

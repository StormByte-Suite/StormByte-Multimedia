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
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/pipeline/side_data.hxx>
#include <StormByte/multimedia/property/audio.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/property/video.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/**
 * @namespace StormByte::Multimedia::Pipeline
 * @brief Demux / decode / filter / encode / mux types.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Pipeline {
	/**
	 * @class Frame
	 * @brief One decoded access unit.
	 *
	 * In libav that is an @c AVFrame: one picture or one block of
	 * samples after decode. In Multimedia it is what process and
	 * analytics filters, and @ref Encoder, see — pixels or samples,
	 * timestamps and the stream tags the decoder copied across.
	 *
	 * Copy and @ref Clone reference libav plane buffers
	 * (`av_frame_ref`). They are not a deep copy of planes.
	 * @ref Filter::FFmpeg::Save emits a new frame when a filter
	 * paints.
	 *
	 * @see Item
	 * @see Packet
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Frame: public Item {
		friend class Backend::Pipeline::Frame;
		friend class Decoder;
		friend class Encoder;
		friend class Filter::FFmpeg;
		friend Decoder& operator>>(Decoder& decoder, Frame& frame) noexcept;
		friend Frame& operator>>(Frame& frame, Encoder& encoder) noexcept;

		public:
			using PointerType = StormByte::Safe::Shared<Frame>;

			/**
			 * @name Construction
			 * @{
			 */

			/**
			 * @brief Empty frame.
			 *
			 * @ref Item::Type is @ref StormByte::Multimedia::Type::Unknown.
			 * @ref Item::Track is -1. @ref Item::Kind is @ref Kind::Frame.
			 */
			Frame() noexcept;

			/**
			 * @brief Builds a frame without a backend buffer.
			 * @param track Origin container stream index.
			 * @param type Media of this unit (Video, Audio or Subtitle).
			 * @param producer Step that created this unit.
			 * @param payload Owned sample / plane bytes.
			 * @param pts Presentation timestamp, if known.
			 * @param duration Frame duration, if known.
			 * @param video Video properties, if this is a video frame.
			 * @param attachments Raw side-data blobs.
			 * @param audio Audio properties, if this is an audio frame.
			 * @param serial Lineage id born at the demuxer for this origin track.
			 * @param part Sub-id inside @p serial. Zero when the demuxed unit did not split.
			 *
			 * @ref Dts starts empty. @ref Decoder stamps it from the
			 * packet that produced this frame. There is no public setter.
			 */
			Frame(int track, enum StormByte::Multimedia::Type type, enum Producer producer,
				StormByte::Buffer::FIFO payload,
				std::optional<Property::Duration> pts,
				std::optional<Property::Duration> duration,
				std::optional<Property::Video> video,
				std::vector<class SideData> attachments,
				std::optional<Property::Audio> audio,
				std::uint64_t serial,
				std::uint64_t part) noexcept;

			/**
			 * @brief Copy. Metadata and FIFO handle are copied; the
			 *        libav frame is referenced (`av_frame_ref`).
			 * @param other Source frame.
			 *
			 * Not a deep copy of planes. @ref Clone uses this
			 * constructor. @ref Filter::FFmpeg::Save still emits a
			 * new frame when a filter paints.
			 */
			Frame(const Frame& other) noexcept;

			/**
			 * @brief Move constructor.
			 * @param other Frame to take.
			 *
			 * Container-safe: @p other becomes the empty sentinel
			 * (@ref Item::Type Unknown, track -1, no backend). @c ~Frame
			 * on a moved-from object is a no-op.
			 */
			Frame(Frame&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			~Frame() noexcept override;

			/**
			 * @brief Copy assignment. Same as the copy constructor.
			 * @param other Source frame.
			 * @return *this.
			 */
			Frame& operator=(const Frame& other) noexcept;

			/**
			 * @brief Move assignment.
			 * @param other Frame to take.
			 * @return *this.
			 *
			 * Same as the move constructor: @p other is left empty.
			 */
			Frame& operator=(Frame&& other) noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Timing and identity
			 * @{
			 */

			/**
			 * @brief Presentation timestamp on the stream clock.
			 * @return Pts, or empty.
			 */
			inline const std::optional<Property::Duration>& Pts() const noexcept {
				return m_pts;
			}

			/**
			 * @brief Decode timestamp on the stream clock.
			 *
			 * Copied by @ref Decoder from the compressed packet that
			 * produced this frame. Empty when that packet had no Dts,
			 * or when the unit was not stamped. There is no public
			 * setter; friendship writes @c m_dts.
			 *
			 * @return Dts, or empty.
			 */
			inline const std::optional<Property::Duration>& Dts() const noexcept {
				return m_dts;
			}

			/**
			 * @brief Frame duration on the stream clock.
			 * @return Duration, or empty.
			 */
			inline const std::optional<Property::Duration>& Duration() const noexcept {
				return m_duration;
			}

			/**
			 * @brief Stream language tag copied from File metadata.
			 * @return Language, or empty if the stream had none.
			 */
			inline const std::optional<std::string>& Language() const noexcept {
				return m_language;
			}

			/**
			 * @brief Stream title tag copied from File metadata.
			 * @return Title, or empty if the stream had none.
			 */
			inline const std::optional<std::string>& Title() const noexcept {
				return m_title;
			}

			/**
			 * @}
			 */

			/**
			 * @name Lineage
			 * @{
			 */

			/**
			 * @brief Monotonic lineage id assigned when the unit enters the pipe.
			 * @return Serial born at the demuxer for this origin track; empty on the sentinel.
			 *
			 * This is not a decoded-frame count and not an FFmpeg @c nb_frames /
			 * @c nb_read_frames figure. It identifies one demuxed access unit as it
			 * travels Demuxer → … → Muxer. Downstream stages copy it.
			 * @ref Part distinguishes several frames born from the same serial.
			 */
			inline const std::optional<std::uint64_t>& Serial() const noexcept {
				return m_serial;
			}

			/**
			 * @brief Sub-id inside @ref Serial when one demuxed unit yields several frames.
			 * @return Zero when the unit did not split; 0, 1, … after a split.
			 *
			 * Downstream stages copy this value. It is not a pipe-wide counter.
			 */
			inline std::uint64_t Part() const noexcept {
				return m_part;
			}

			/**
			 * @}
			 */

			/**
			 * @name Properties
			 * @{
			 */

			/**
			 * @brief Video properties (includes HDR10 when set).
			 * @return Video, or empty.
			 */
			inline const std::optional<Property::Video>& Video() const noexcept {
				return m_video;
			}

			/**
			 * @brief Audio properties (layout, rate, channels).
			 * @return Audio, or empty.
			 */
			inline const std::optional<Property::Audio>& Audio() const noexcept {
				return m_audio;
			}

			/**
			 * @brief Raw side data captured at receive. Read-only.
			 * @return Blobs. MDM/CLL also appear in @ref Video() HDR10
			 *         when the decoder could map them.
			 */
			inline const std::vector<class SideData>& Attachments() const noexcept {
				return m_attachments;
			}

			/**
			 * @}
			 */

			/**
			 * @name Payload
			 * @{
			 */

			/**
			 * @brief Payload. Materialises planes on first call if a backend frame is held.
			 * @return FIFO.
			 *
			 * After materialisation the backend stays alive; a later
			 * filter @c Save replaces it and this FIFO is cleared.
			 */
			StormByte::Buffer::FIFO& Payload() noexcept;

			/**
			 * @brief Payload already materialised, or empty.
			 * @return FIFO. Does not pull planes out of the backend.
			 */
			inline const StormByte::Buffer::FIFO& Payload() const noexcept {
				return m_payload;
			}

			/**
			 * @}
			 */

		private:
			/**
			 * @brief Adopts a backend frame for lazy @ref Payload().
			 * @param backend Backend holder.
			 */
			void Bind(std::unique_ptr<Backend::Pipeline::Frame> backend) noexcept;

			/**
			 * @brief Turns this unit into the empty sentinel.
			 *
			 * Used by move construction and move assignment so a
			 * relocated @c Frame in a container is always valid.
			 */
			void BecomeEmpty() noexcept;

			/**
			 * @brief Fork for hopper taps (`Clonable::Clone`).
			 * @return New unit sharing libav plane buffers (`av_frame_ref`).
			 *
			 * Not a public API. Uses the copy constructor. Not a
			 * deep copy of planes.
			 */
			Item::PointerType Clone() const override;

			/**
			 * @brief Moves this unit into a new owning pointer.
			 * @return Pointer to the relocated unit; *this becomes the empty sentinel.
			 *
			 * Not a public API. Uses the move constructor.
			 * @ref Step::Emit does not call this.
			 */
			Item::PointerType Move() override;

			StormByte::Buffer::FIFO m_payload;							///< Sample / subtitle bytes
			std::optional<Property::Duration> m_pts;					///< Presentation timestamp
			std::optional<Property::Duration> m_dts;					///< Decode timestamp from the source packet
			std::optional<Property::Duration> m_duration;				///< Frame duration
			std::optional<Property::Video> m_video;						///< Video properties
			std::optional<Property::Audio> m_audio;						///< Audio properties
			std::optional<std::string> m_language;						///< Stream language tag
			std::optional<std::string> m_title;							///< Stream title tag
			std::vector<class SideData> m_attachments;					///< Raw side data
			std::optional<std::uint64_t> m_serial;						///< Lineage id born at demux
			std::uint64_t m_part;										///< Sub-id inside serial
			std::unique_ptr<Backend::Pipeline::Frame> m_backend;		///< Backend holder
	};
}

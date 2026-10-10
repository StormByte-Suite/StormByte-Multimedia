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

#include <StormByte/buffer/fifo.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/pipeline/side_data.hxx>
#include <StormByte/multimedia/property/audio.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/property/video.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/vector.hxx>

#include <cstdint>
#include <memory>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace Multimedia
	 * @brief Multimedia module of the StormByte suite.
	 */
	namespace Multimedia {
		/**
		 * @namespace Pipeline
		 * @brief Pipeline Demux / decode / filter / encode / mux types.
		 *
		 * @ingroup multimedia_pipeline
		 */
		namespace Pipeline {
			/**
			 * @class Frame
			 * @brief One decoded access unit.
			 *
			 * One picture or one block of decoded samples, consumed by process
			 * and analytics filters and @ref Encoder. Provides pixels or samples,
			 * timestamps and the stream tags the decoder copied across.
			 *
			 * Copies and @ref Clone share media buffers rather than duplicating
			 * pixels or samples.
			 * Metadata accessors borrow their wrappers until this frame is modified or
			 * destroyed. Safe optional arrow access returns a read-only snapshot for the
			 * current full expression, not a stable pointer into the frame.
			 *
			 * @see Item
			 * @see Packet
			 *
			 * @ingroup multimedia_pipeline
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Frame: public Item {
				public:
					/**
					 * @brief Base-heap shared owner of a frame; keep its providers loaded.
					 */
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
					 * @throws StormByte::Exception If safe metadata storage cannot be allocated.
					 */
					Frame();

					/**
					 * @brief Builds a frame from supplied media bytes and properties.
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
					 * @throws StormByte::Exception If safe metadata storage cannot be allocated.
					 */
					Frame(int track, enum StormByte::Multimedia::Type type, enum Producer producer,
						StormByte::Buffer::FIFO payload,
						StormByte::Safe::Optional<Property::Duration> pts,
						StormByte::Safe::Optional<Property::Duration> duration,
						StormByte::Safe::Optional<Property::Video> video,
						StormByte::Safe::Vector<class SideData> attachments,
						StormByte::Safe::Optional<Property::Audio> audio,
						std::uint64_t serial,
						std::uint64_t part);

					/**
					 * @brief Copy. Metadata and FIFO handle are copied; the
					 *        libav frame is referenced (`av_frame_ref`).
					 * @param other Source frame.
					 *
					 * Not a deep copy of planes. @ref Clone uses this
					 * constructor. @ref Filter::FFmpeg::Save still emits a
					 * new frame when a filter paints.
					 * @throws StormByte::Exception If safe metadata storage cannot be copied.
					 * @throws std::bad_alloc If the private backend holder cannot be allocated.
					 */
					Frame(const Frame& other);

					/**
					 * @brief Move constructor.
					 * @param other Frame to take.
					 *
					 * Container-safe: @p other becomes the empty sentinel
					 * (@ref Item::Type Unknown, track -1, no media). @c ~Frame
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
					 * @note The destination is unchanged if copying fails.
					 * @throws StormByte::Exception If safe metadata storage cannot be copied.
					 * @throws std::bad_alloc If the private backend holder cannot be allocated.
					 */
					Frame& operator=(const Frame& other);

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
					inline const StormByte::Safe::Optional<Property::Duration>& Pts() const noexcept {
						return m_pts;
					}

					/**
					 * @brief Decode timestamp on the stream clock.
					 *
					 * Copied by @ref Decoder from the compressed packet that
					 * produced this frame. Empty when that packet had no Dts,
					 * or when the unit was not stamped. There is no public
					 * setter.
					 *
					 * @return Dts, or empty.
					 */
					inline const StormByte::Safe::Optional<Property::Duration>& Dts() const noexcept {
						return m_dts;
					}

					/**
					 * @brief Frame duration on the stream clock.
					 * @return Duration, or empty.
					 */
					inline const StormByte::Safe::Optional<Property::Duration>& Duration() const noexcept {
						return m_duration;
					}

					/**
					 * @brief Stream language tag copied from File metadata.
					 * @return Language, or empty if the stream had none.
					 */
					inline const StormByte::Safe::Optional<StormByte::Safe::String>& Language() const noexcept {
						return m_language;
					}

					/**
					 * @brief Stream title tag copied from File metadata.
					 * @return Title, or empty if the stream had none.
					 */
					inline const StormByte::Safe::Optional<StormByte::Safe::String>& Title() const noexcept {
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
					inline const StormByte::Safe::Optional<std::uint64_t>& Serial() const noexcept {
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
					inline const StormByte::Safe::Optional<Property::Video>& Video() const noexcept {
						return m_video;
					}

					/**
					 * @brief Audio properties (layout, rate, channels).
					 * @return Audio, or empty.
					 */
					inline const StormByte::Safe::Optional<Property::Audio>& Audio() const noexcept {
						return m_audio;
					}

					/**
					 * @brief Raw side data captured at receive. Read-only.
					 * @return Blobs. MDM/CLL also appear in @ref Video() HDR10
					 *         when the decoder could map them.
					 */
					inline const StormByte::Safe::Vector<class SideData>& Attachments() const noexcept {
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
					 * @brief Payload. Materializes decoded media bytes on first access.
					 * @return FIFO.
					 *
					 * Materialization preserves the decoded media. Filtering that
					 * replaces the media invalidates the previously materialized bytes.
					 */
					StormByte::Buffer::FIFO& Payload() noexcept;

					/**
					 * @brief Payload already materialised, or empty.
					 * @return FIFO. Does not materialize decoded media bytes.
					 */
					inline const StormByte::Buffer::FIFO& Payload() const noexcept {
						return m_payload;
					}

					/**
					 * @}
					 */

				private:
					/**
					 * @brief Allows the backend holder to access the frame's media storage.
					 */
					friend class Backend::Pipeline::Frame;

					/**
					 * @brief Allows the decoder to populate decoded media and metadata.
					 */
					friend class Decoder;

					/**
					 * @brief Allows the encoder to access the backend frame.
					 */
					friend class Encoder;

					/**
					 * @brief Allows FFmpeg filters to update decoded media and metadata.
					 */
					friend class Filter::FFmpeg;

					/**
					 * @brief Allows the extraction operator to fill a frame from a decoder.
					 * @param decoder Decoder supplying decoded media.
					 * @param frame Destination frame.
					 * @return Decoder supplying the frame.
					 */
					friend Decoder& operator>>(Decoder& decoder, Frame& frame) noexcept;

					/**
					 * @brief Allows the insertion operator to submit a frame to an encoder.
					 * @param frame Frame supplying decoded media.
					 * @param encoder Encoder receiving the frame.
					 * @return Submitted frame.
					 */
					friend Frame& operator>>(Frame& frame, Encoder& encoder) noexcept;

					/**
					 * @brief Adopts a backend frame for lazy @ref Payload().
					 * @param backend Backend holder.
					 */
					void Bind(StormByte::Safe::Unique<Backend::Pipeline::Frame> backend) noexcept;

					/**
					 * @brief Turns this unit into the empty sentinel.
					 *
					 * Used by move construction and move assignment so a
					 * relocated @c Frame in a container is always valid.
					 * @pre All owning wrappers have already been moved to the destination.
					 * Their moved-from state is empty; no allocating clear/reset is performed.
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

					StormByte::Buffer::FIFO m_payload;				///< Buffer-owned materialized sample or subtitle bytes.
					StormByte::Safe::Optional<Property::Duration> m_pts;		///< Presentation timestamp in Base-owned optional storage.
					StormByte::Safe::Optional<Property::Duration> m_dts;		///< Decode timestamp copied from the source packet.
					StormByte::Safe::Optional<Property::Duration> m_duration;		///< Optional frame duration.
					StormByte::Safe::Optional<Property::Video> m_video;		///< Optional provider-owned video properties.
					StormByte::Safe::Optional<Property::Audio> m_audio;		///< Optional provider-owned audio properties.
					StormByte::Safe::Optional<StormByte::Safe::String> m_language;	///< Optional Base-owned stream language tag.
					StormByte::Safe::Optional<StormByte::Safe::String> m_title;		///< Optional Base-owned stream title tag.
					StormByte::Safe::Vector<class SideData> m_attachments;		///< Opaque safe sequence of independently copied side-data blobs.
					StormByte::Safe::Optional<std::uint64_t> m_serial;			///< Lineage identifier, empty on the sentinel.
					std::uint64_t m_part;						///< Sub-identifier within the lineage, zero on the sentinel.
					StormByte::Safe::Unique<Backend::Pipeline::Frame> m_backend;	///< Multimedia-private FFmpeg frame holder.
			};
		}
	}
}

/**
 * @brief Registers the completed frame under Multimedia's conditional ABI contract.
 *
 * Metadata uses Base-owned Safe wrappers, payload uses Buffer, and the private
 * backend is managed by exported Multimedia lifetime operations. Compatible
 * compiler and runtime ABIs are required; providers must outlive all frames and
 * callback-backed wrappers. This is not a certification of external subclasses.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Frame);

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
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/binary.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/vector.hxx>

#include <cstdint>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Portable multimedia preview contracts.
	 */
	namespace Multimedia {
		/**
		 * @struct PreviewVideo
		 * @brief Owned, tightly packed RGBA8 preview image, not an HDR mastering surface.
		 */
		struct STORMBYTE_MULTIMEDIA_PUBLIC PreviewVideo {
			/**
			 * @brief Construct an empty image.
			 */
			PreviewVideo() = default;

			/**
			 * @brief Copy an image.
			 * @param other Source.
			 */
			PreviewVideo(const PreviewVideo& other) = default;

			/**
			 * @brief Move an image.
			 * @param other Source.
			 */
			PreviewVideo(PreviewVideo&& other) noexcept = default;

			/**
			 * @brief Release image storage.
			 */
			~PreviewVideo() noexcept = default;

			/**
			 * @brief Copy an image.
			 * @param other Source.
			 * @return This image.
			 */
			PreviewVideo& operator=(const PreviewVideo& other) = default;

			/**
			 * @brief Move an image.
			 * @param other Source.
			 * @return This image.
			 */
			PreviewVideo& operator=(PreviewVideo&& other) noexcept = default;

			Safe::Binary m_pixels;		///< Row-major RGBA8 bytes; stride is m_width * 4.
			std::uint32_t m_width = 0;	///< Pixel width.
			std::uint32_t m_height = 0;	///< Pixel height.
			std::uint64_t m_epoch = 0;	///< Delivery generation.
			double m_position = 0;		///< Presentation time in media seconds.
			double m_duration = 0;		///< Image duration in seconds.
		};

		/**
		 * @enum AudioSample
		 * @brief Decoded native-endian PCM representation, without conversion.
		 */
		enum class AudioSample {
			Unsigned8,	///< Unsigned 8-bit.
			Signed16,	///< Signed 16-bit.
			Signed32,	///< Signed 32-bit.
			Signed64,	///< Signed 64-bit.
			Float32,	///< IEEE 32-bit float.
			Float64		///< IEEE 64-bit float.
		};

		/**
		 * @enum AudioLayout
		 * @brief Interpretation of ordered decoded channel identities.
		 */
		enum class AudioLayout {
			Unspecified,	///< Count only; speaker mapping is unknown.
			Speakers,		///< Known native speaker order.
			Custom,			///< Explicit decoder channel order; identities may be unknown.
			Ambisonic		///< Ambisonic components, not ordinary speakers.
		};

		/**
		 * @struct PreviewAudio
		 * @brief Losslessly owned decoded PCM, at its actual rate and channel order.
		 *
		 * Planar blocks contain one plane per channel; packed blocks contain one
		 * interleaved plane. Planes contain only sample bytes, never decoder padding.
		 * Channel identities use canonical speaker abbreviations (FL, FR, FC, LFE,
		 * BL, BR, SL, SR, etc.), AMBI followed by the ACN component number, or
		 * NONE (unspecified), UNK (unknown) or USR identifiers. BL/BR and SL/SR
		 * are distinct. Unspecified,
		 * unknown and ambisonic channels must never be guessed by a device adapter.
		 * The Display owns conversion, reordering and device negotiation.
		 */
		struct STORMBYTE_MULTIMEDIA_PUBLIC PreviewAudio {
			/**
			 * @brief Construct an empty block.
			 */
			PreviewAudio() = default;

			/**
			 * @brief Copy samples.
			 * @param other Source.
			 */
			PreviewAudio(const PreviewAudio& other) = default;

			/**
			 * @brief Move samples.
			 * @param other Source.
			 */
			PreviewAudio(PreviewAudio&& other) noexcept = default;

			/**
			 * @brief Release samples.
			 */
			~PreviewAudio() noexcept = default;

			/**
			 * @brief Copy samples.
			 * @param other Source.
			 * @return This block.
			 */
			PreviewAudio& operator=(const PreviewAudio& other) = default;

			/**
			 * @brief Move samples.
			 * @param other Source.
			 * @return This block.
			 */
			PreviewAudio& operator=(PreviewAudio&& other) noexcept = default;

			Safe::Vector<Safe::Binary> m_planes;	///< Exact owned sample planes.
			Safe::Vector<Safe::String> m_channels;	///< Identities in decoded sample order; size is channel count.
			AudioSample m_format = AudioSample::Unsigned8;	///< Actual decoded sample representation.
			AudioLayout m_layout = AudioLayout::Unspecified;	///< Channel identity interpretation.
			bool m_planar = false;		///< Separate channel planes rather than packed samples.
			std::uint32_t m_rate = 0;	///< Actual decoded samples per second per channel.
			std::uint32_t m_frames = 0;	///< Samples per channel.
			std::uint64_t m_epoch = 0;	///< Delivery generation.
			double m_position = 0;		///< First sample presentation time in media seconds.
		};

		/**
		 * @enum SubtitleContent
		 * @brief FFmpeg decoded subtitle representation.
		 */
		enum class SubtitleContent {
			Text,	///< UTF-8 plain text.
			Ass,	///< ASS event with the decoder's style header.
			Bitmap	///< RGBA8 pixels at the supplied canvas coordinates.
		};

		/**
		 * @struct SubtitleRegion
		 * @brief One decoded subtitle rectangle.
		 */
		struct STORMBYTE_MULTIMEDIA_PUBLIC SubtitleRegion {
			/**
			 * @brief Construct an empty rectangle.
			 */
			SubtitleRegion() = default;

			/**
			 * @brief Copy a rectangle.
			 * @param other Source.
			 */
			SubtitleRegion(const SubtitleRegion& other) = default;

			/**
			 * @brief Move a rectangle.
			 * @param other Source.
			 */
			SubtitleRegion(SubtitleRegion&& other) noexcept = default;

			/**
			 * @brief Release rectangle storage.
			 */
			~SubtitleRegion() noexcept = default;

			/**
			 * @brief Copy a rectangle.
			 * @param other Source.
			 * @return This rectangle.
			 */
			SubtitleRegion& operator=(const SubtitleRegion& other) = default;

			/**
			 * @brief Move a rectangle.
			 * @param other Source.
			 * @return This rectangle.
			 */
			SubtitleRegion& operator=(SubtitleRegion&& other) noexcept = default;

			SubtitleContent m_content = SubtitleContent::Text;	///< Interpretation of this rectangle.
			Safe::String m_text;								///< UTF-8 text or an ASS event.
			Safe::Binary m_pixels;								///< RGBA8 bitmap, tightly packed.
			int m_x = 0;										///< Canvas left.
			int m_y = 0;										///< Canvas top.
			int m_width = 0;									///< Bitmap width.
			int m_height = 0;									///< Bitmap height.
		};
	}
}

/**
 * @brief Admit rectangles containing only Safe owners and scalar values.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::SubtitleRegion);

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Portable multimedia preview contracts.
	 */
	namespace Multimedia {
		/**
		 * @struct PreviewSubtitle
		 * @brief Timed decoded subtitle, retaining text, bitmap and styling information.
		 */
		struct STORMBYTE_MULTIMEDIA_PUBLIC PreviewSubtitle {
			/**
			 * @brief Construct an empty cue.
			 */
			PreviewSubtitle() = default;

			/**
			 * @brief Copy a cue.
			 * @param other Source.
			 */
			PreviewSubtitle(const PreviewSubtitle& other) = default;

			/**
			 * @brief Move a cue.
			 * @param other Source.
			 */
			PreviewSubtitle(PreviewSubtitle&& other) noexcept = default;

			/**
			 * @brief Release cue storage.
			 */
			~PreviewSubtitle() noexcept = default;

			/**
			 * @brief Copy a cue.
			 * @param other Source.
			 * @return This cue.
			 */
			PreviewSubtitle& operator=(const PreviewSubtitle& other) = default;

			/**
			 * @brief Move a cue.
			 * @param other Source.
			 * @return This cue.
			 */
			PreviewSubtitle& operator=(PreviewSubtitle&& other) noexcept = default;

			Safe::Vector<SubtitleRegion> m_regions;					///< Decoded rectangles; empty means clear.
			Safe::String m_style_header;							///< ASS/SSA decoder header, if supplied.
			Safe::Vector<Safe::Shared<const Attachment>> m_fonts;	///< Shared embedded TTF/OTF resources for styled rendering.
			int m_canvas_width = 0;									///< Subtitle canvas width; zero if unspecified.
			int m_canvas_height = 0;								///< Subtitle canvas height; zero if unspecified.
			std::uint64_t m_epoch = 0;								///< Delivery generation.
			std::uint64_t m_id = 0;									///< Cue identity within the epoch.
			double m_start = 0;										///< Inclusive start in media seconds.
			double m_end = 0;										///< Exclusive end; infinity means until a matching clear or stream drain.
		};

		/**
		 * @struct AudioPresentation
		 * @brief Device clock feedback in media seconds for the requested epoch.
		 *
		 * Audible position is m_position minus m_latency. m_valid becomes true after
		 * audio starts; the clock must advance through the last submitted sample,
		 * freeze during pause and never describe another epoch.
		 */
		struct AudioPresentation {
			/**
			 * @brief Construct unavailable clock feedback.
			 */
			AudioPresentation() = default;

			/**
			 * @brief Copy clock feedback.
			 * @param other Source.
			 */
			AudioPresentation(const AudioPresentation& other) = default;

			/**
			 * @brief Move clock feedback.
			 * @param other Source.
			 */
			AudioPresentation(AudioPresentation&& other) noexcept = default;

			/**
			 * @brief Destroy clock feedback.
			 */
			~AudioPresentation() noexcept = default;

			/**
			 * @brief Copy clock feedback.
			 * @param other Source.
			 * @return This feedback.
			 */
			AudioPresentation& operator=(const AudioPresentation& other) = default;

			/**
			 * @brief Move clock feedback.
			 * @param other Source.
			 * @return This feedback.
			 */
			AudioPresentation& operator=(AudioPresentation&& other) noexcept = default;

			bool m_valid = false;	///< Whether device feedback is available.
			bool m_drained = false;	///< Whether all submitted audio samples have been heard (not GUI drain).
			double m_position = 0;	///< Device playback cursor in media seconds.
			double m_latency = 0;	///< Unheard device latency in seconds, nonnegative.
		};

		/**
		 * @class Display
		 * @brief Consumer-owned combined video, audio and subtitle presentation sink.
		 *
		 * Callbacks run on worker or control-caller threads and must be bounded
		 * and thread-safe. Do not reenter any player method. Throw on device or
		 * delivery failure. Maintain bounded audio and GUI queues; audio starts
		 * at most 150 ms ahead, plus the duration of the submitted block.
		 * Invalidate synchronously publishes a new epoch, flushes audio and cues,
		 * and clears the old image even when video remains active. Queued GUI
		 * deliveries must check that epoch at execution, not merely enqueue time.
		 * The consumer implements its Qt/SDL/native integration; destruction of a
		 * player never closes this shared widget or its application.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC Display {
			public:
				/**
				 * @brief Construct a sink.
				 */
				Display() = default;

				Display(const Display&) = delete;

				Display(Display&&) = delete;

				/**
				 * @brief Release this sink, not managed by player Close.
				 */
				virtual ~Display() noexcept = default;

				Display& operator=(const Display&) = delete;

				Display& operator=(Display&&) = delete;

				/**
				 * @brief Flush presentation and synchronously invalidate old deliveries.
				 * @param epoch New generation.
				 * @param active_video Whether an ordinary video track is selected.
				 */
				virtual void Invalidate(std::uint64_t epoch, bool active_video) = 0;

				/**
				 * @brief Freeze or resume device audio and subtitle timers; invalidate an old pause timer.
				 * @param paused Whether presentation is paused.
				 * @param epoch Current generation.
				 */
				virtual void Pause(bool paused, std::uint64_t epoch) = 0;

				/**
				 * @brief Present an owned image after its scheduled time.
				 * @param image Image; reject stale epochs at GUI execution.
				 */
				virtual void Video(Safe::Shared<const PreviewVideo> image) = 0;

				/**
				 * @brief Queue a PCM block without blocking for playback.
				 * @param audio Owned samples with timeline position.
				 */
				virtual void Audio(Safe::Shared<const PreviewAudio> audio) = 0;

				/**
				 * @brief Present a cue; expire it at m_end on the supplied media clock.
				 * @param subtitle Owned cue. Preserve overlapping cue identities.
				 *
				 * Empty m_regions clears m_id, or all cues when m_id is zero. Infinite m_end
				 * remains active until such a clear; Position expires finite cues.
				 */
				virtual void Subtitle(Safe::Shared<const PreviewSubtitle> subtitle) = 0;

				/**
				 * @brief Read current epoch's audio device cursor and latency.
				 * @param epoch Current generation.
				 * @return Device clock feedback.
				 */
				virtual AudioPresentation AudioClock(std::uint64_t epoch) = 0;

				/**
				 * @brief Advance subtitle timing, including when audio is disabled.
				 * @param position Media seconds; frozen during pause.
				 * @param epoch Current generation.
				 */
				virtual void Position(double position, std::uint64_t epoch) = 0;

				/**
				 * @brief Whether all queued audio and GUI deliveries have completed.
				 * @param epoch Current generation.
				 * @return True only after presentation has drained.
				 */
				virtual bool Drained(std::uint64_t epoch) = 0;
		};
	}
}

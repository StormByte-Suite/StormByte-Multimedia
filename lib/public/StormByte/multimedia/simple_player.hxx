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

#include <StormByte/buffer/io/buffered_location_reader.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/multimedia/display.hxx>
#include <StormByte/multimedia/type.hxx>

#include <string_view>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Portable multimedia services.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Backend
		 * @brief Private playback engine.
		 */
		namespace Backend {
			/**
			 * @brief Private player implementation.
			 */
			class SimplePlayer;
		}
	}
}

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Portable multimedia services.
	 */
	namespace Multimedia {
		/**
		 * @enum PlaybackState
		 * @brief Observable preview lifecycle.
		 */
		enum class PlaybackState {
			Closed,		///< No input.
			Stopped,	///< Open input, rewound.
			Playing,	///< Asynchronous playback.
			Paused,		///< Frozen presentation, including paused seek previews.
			Ended,		///< Decode and presentation drained.
			Error		///< Explicit failure; Error() provides details.
		};

		/**
		 * @struct PreviewTrack
		 * @brief Selectable input stream, excluding attached pictures and attachments.
		 */
		struct STORMBYTE_MULTIMEDIA_PUBLIC PreviewTrack {
			/**
			 * @brief Construct an invalid track.
			 */
			PreviewTrack() = default;

			/**
			 * @brief Copy track information.
			 * @param other Source.
			 */
			PreviewTrack(const PreviewTrack& other) = default;

			/**
			 * @brief Move track information.
			 * @param other Source.
			 */
			PreviewTrack(PreviewTrack&& other) noexcept = default;

			/**
			 * @brief Release track text.
			 */
			~PreviewTrack() noexcept = default;

			/**
			 * @brief Copy track information.
			 * @param other Source.
			 * @return This track.
			 */
			PreviewTrack& operator=(const PreviewTrack& other) = default;

			/**
			 * @brief Move track information.
			 * @param other Source.
			 * @return This track.
			 */
			PreviewTrack& operator=(PreviewTrack&& other) noexcept = default;

			int m_index = -1;					///< Demux stream index.
			enum Type m_type = Type::Unknown;	///< Video, audio or subtitle.
			Safe::String m_language;			///< Container language tag.
			Safe::String m_title;				///< Container title tag.
			Safe::String m_codec;				///< Decoder codec name.
		};
	}
}

/**
 * @brief Admit track snapshots containing only Safe strings and scalar values.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::PreviewTrack);

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Portable multimedia services.
	 */
	namespace Multimedia {
		/**
		 * @class SimplePlayer
		 * @brief Real-time asynchronous file preview, not a professional media player.
		 *
		 * Control operations are serialized internally. Display callbacks must not
		 * reenter player methods. Bool failures and worker failures set Error(); inspect
		 * State() while playing. Owns exactly one buffered reader, retaining its
		 * dynamic type. Display is mandatory; no native presentation backend is
		 * bundled. All provider libraries must outlive Safe owners.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC SimplePlayer {
			public:
				/**
				 * @brief Construct a preview engine with an application sink.
				 * @param display Nonempty combined presentation sink.
				 * @param logger Optional shared logger.
				 * @throws StormByte::Exception If display is empty.
				 */
				explicit SimplePlayer(Safe::Shared<Display> display, Safe::Shared<Logger::Log> logger = {});

				SimplePlayer(const SimplePlayer&) = delete;

				SimplePlayer(SimplePlayer&&) = delete;

				/**
				 * @brief Stop/join workers, close input, release owners without rewinding.
				 */
				~SimplePlayer() noexcept;

				SimplePlayer& operator=(const SimplePlayer&) = delete;

				SimplePlayer& operator=(SimplePlayer&&) = delete;

				/**
				 * @brief Open a locally owned BufferedFileReader; replace any old input.
				 * @param file Borrowed path, copied only into the reader's Safe::String.
				 * @return Whether probing and initial decoder selection succeeded.
				 */
				bool Open(std::string_view file);

				/**
				 * @brief Exclusively take an input; open only if not already open.
				 * @param reader Dynamic reader owner. Retained even if opening fails.
				 * @return Whether input and decoders are usable.
				 */
				bool Open(Safe::Unique<Buffer::IO::BufferedLocationReader> reader);

				/**
				 * @brief Start/resume asynchronous real-time presentation.
				 * @return Whether playback started; Ended requires Seek or Stop first.
				 */
				bool Play();

				/**
				 * @brief Freeze workers and presentation, retaining queued decode.
				 * @return Whether pause succeeded.
				 */
				bool Pause();

				/**
				 * @brief Join workers and rewind the actual reader, keeping it open.
				 * @return Whether rewind succeeded; failure retains ownership.
				 */
				bool Stop();

				/**
				 * @brief Seek and flush decoders, devices, cues and stale GUI deliveries.
				 * @param seconds Nonnegative media seconds. Paused seek displays a new image.
				 * @return Whether seek started successfully; asynchronous errors remain observable.
				 */
				bool Seek(double seconds);

				/**
				 * @brief Switch or disable a media kind at the current position.
				 * @param type Video, audio or subtitle.
				 * @param index Stream index, or -1 to disable.
				 * @return Whether decoder selection and seek succeeded.
				 */
				bool SelectTrack(enum Type type, int index);

				/**
				 * @brief Read selected stream index.
				 * @param type Media kind.
				 * @return Stream index, or -1 when disabled.
				 */
				int SelectedTrack(enum Type type) const;

				/**
				 * @brief Snapshot selectable stream information.
				 * @return Safe-owned track list.
				 */
				Safe::Vector<PreviewTrack> Tracks() const;

				/**
				 * @brief Snapshot lifecycle state.
				 * @return Current state.
				 */
				PlaybackState State() const;

				/**
				 * @brief Snapshot audible/video timeline position.
				 * @return Media seconds.
				 */
				double Position() const;

				/**
				 * @brief Read container duration, if known.
				 * @return Seconds, or zero when unavailable.
				 */
				double Duration() const;

				/**
				 * @brief Snapshot the last explicit failure.
				 * @return Safe-owned diagnostic, empty when none.
				 */
				Safe::String Error() const;

				/**
				 * @brief Join workers and close/release input, without rewind.
				 * @return Whether presentation invalidation and reader close succeeded.
				 */
				bool Close();

				/**
				 * @brief Join workers, rewind and transfer the still-open dynamic reader.
				 * @return Owner, or empty on failure with ownership retained by the player.
				 *
				 * Never closes or reopens a faulted reader as a fallback. Stop/transfer
				 * reject faulted sessions before Rewind to preserve remote mount tokens.
				 */
				Safe::Unique<Buffer::IO::BufferedLocationReader> ReleaseReader();

			private:
				Safe::Unique<Backend::SimplePlayer> m_backend;	///< Provider-owned engine.
		};
	}
}

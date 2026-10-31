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

#include <StormByte/multimedia/simple_player.hxx>

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
		 * @brief Private portable FFmpeg engine.
		 */
		namespace Backend {
			/**
			 * @class SimplePlayer
			 * @brief Player implementation and serialized input lifecycle.
			 */
			class STORMBYTE_MULTIMEDIA_PRIVATE SimplePlayer {
				public:
					/**
					 * @brief Retain presentation and logging owners.
					 * @param display Nonempty sink.
					 * @param logger Optional logger.
					 */
					SimplePlayer(Safe::Shared<Display> display, Safe::Shared<Logger::Log> logger);

					SimplePlayer(const SimplePlayer&) = delete;

					SimplePlayer(SimplePlayer&&) = delete;

					/**
					 * @brief Join workers and close without rewind; never throw.
					 */
					~SimplePlayer() noexcept;

					SimplePlayer& operator=(const SimplePlayer&) = delete;

					SimplePlayer& operator=(SimplePlayer&&) = delete;

					/**
					 * @brief Take and probe input.
					 * @param reader Exclusive origin owner.
					 * @return Success.
					 */
					bool Open(Safe::Unique<Buffer::IO::BufferedLocationReader> reader);

					/**
					 * @brief Start or resume workers.
					 * @return Success.
					 */
					bool Play();

					/**
					 * @brief Freeze workers and device clock.
					 * @return Success.
					 */
					bool Pause();

					/**
					 * @brief Join, invalidate and rewind.
					 * @return Success.
					 */
					bool Stop();

					/**
					 * @brief Change timeline with flushed decode.
					 * @param seconds Target.
					 * @return Success.
					 */
					bool Seek(double seconds);

					/**
					 * @brief Select a decoder and reseek.
					 * @param type Media kind.
					 * @param index Stream index or -1.
					 * @return Success.
					 */
					bool SelectTrack(enum Type type, int index);

					/**
					 * @brief Snapshot selection.
					 * @param type Media kind.
					 * @return Index or -1.
					 */
					int SelectedTrack(enum Type type) const;

					/**
					 * @brief Snapshot tracks.
					 * @return Safe-owned information.
					 */
					Safe::Vector<PreviewTrack> Tracks() const;

					/**
					 * @brief Snapshot state.
					 * @return Lifecycle.
					 */
					PlaybackState State() const;

					/**
					 * @brief Snapshot position.
					 * @return Seconds.
					 */
					double Position() const;

					/**
					 * @brief Snapshot duration.
					 * @return Seconds or zero.
					 */
					double Duration() const;

					/**
					 * @brief Snapshot diagnostic.
					 * @return Safe text.
					 */
					Safe::String Error() const;

					/**
					 * @brief Join, invalidate and close.
					 * @return Success.
					 */
					bool Close();

					/**
					 * @brief Rewind and transfer, retaining on failure.
					 * @return Open owner or empty.
					 */
					Safe::Unique<Buffer::IO::BufferedLocationReader> ReleaseReader();

				private:
					/**
					 * @brief Implementation state and workers.
					 */
					class Context;
					Safe::Unique<Context> m_ctx;	///< Provider-owned state.
			};
		}
	}
}

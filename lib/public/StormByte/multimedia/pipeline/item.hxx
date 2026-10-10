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

#include <StormByte/multimedia/pipeline/typedefs.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/clonable.hxx>

#include <cstdint>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @brief Decoded-AU holder behind @ref StormByte::Multimedia::Pipeline::Frame.
	 */
	class Frame;

	/**
	 * @brief Compressed-AU holder behind @ref StormByte::Multimedia::Pipeline::Packet.
	 */
	class Packet;

	/**
	 * @brief In / out hoppers; CloneTo forks through Item::Clone.
	 */
	class Pipe;
}

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Multimedia module of the StormByte suite.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Pipeline
		 * @brief Pipeline Demux / decode / filter / encode / mux types.
		 *
		 * @ingroup multimedia_pipeline
		 */
		namespace Pipeline {
			/**
			 * @brief Decoding stage for compressed access units.
			 */
			class Decoder;

			/**
			 * @brief Demultiplexing stage for input containers.
			 */
			class Demuxer;

			/**
			 * @brief Encoding stage for decoded access units.
			 */
			class Encoder;

			/**
			 * @brief Decoded access-unit facade.
			 */
			class Frame;

			/**
			 * @brief Multiplexing stage for output containers.
			 */
			class Muxer;

			/**
			 * @brief Compressed access-unit facade.
			 */
			class Packet;

			/**
			 * @brief Shared pipeline-stage interface.
			 */
			class Step;

			/**
			 * @namespace StormByte::Multimedia::Pipeline::Filter
			 * @brief Frame and packet filters attached to a route.
			 *
			 * @ingroup multimedia_pipeline
			 */
			namespace Filter {
				/**
				 * @brief Filter base. Not a leaf: inherit Process, Packet or Analytics.
				 */
				class FFmpeg;
			}

			/**
			 * @class Item
			 * @brief Facade shared by Frame and Packet.
			 *
			 * @ref Clone (via @c StormByte::Safe::Clonable) returns a new
			 * @c StormByte::Safe::Shared and
			 * shares media buffers. It is not a deep copy of planes or packet
			 * payload. @ref Move relocates the unit.
			 *
			 * @ingroup multimedia_pipeline
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Item: protected StormByte::Safe::Clonable<Item> {
				public:
					/**
					 * @brief Base-heap owner returned by internal polymorphic cloning.
					 */
					using StormByte::Safe::Clonable<Item>::PointerType;

					/**
					 * @name Lifecycle
					 * @{
					 */

					/**
					 * @brief Destructor.
					 */
					virtual ~Item() noexcept = default;

					/**
					 * @}
					 */

					/**
					 * @name Identity
					 * @{
					 */

					/**
					 * @brief Whether this unit is a frame or a packet.
					 * @return Kind::Frame or Kind::Packet.
					 */
					Kind Kind() const noexcept {
						return m_kind;
					}

					/**
					 * @brief Media of this access unit.
					 * @return Video, Audio, Subtitle, or Unknown on the empty sentinel.
					 */
					enum StormByte::Multimedia::Type Type() const noexcept {
						return m_type;
					}

					/**
					 * @brief Origin track index.
					 * @return Input container stream index. -1 on the empty sentinel.
					 */
					int Track() const noexcept {
						return m_track;
					}

					/**
					 * @brief Stage that created this unit.
					 * @return Producer associated with this unit.
					 */
					enum Producer Producer() const noexcept {
						return m_producer;
					}

					/**
					 * @}
					 */

				protected:
					/**
					 * @name Lifecycle
					 * @{
					 */

					/**
					 * @brief Copy constructor. Identity fields only; media lives on Frame / Packet.
					 * @param other Source item.
					 */
					Item(const Item& other) noexcept = default;

					/**
					 * @brief Move constructor.
					 * @param other Item to take.
					 */
					Item(Item&& other) noexcept = default;

					/**
					 * @brief Copy assignment. Identity fields only; media lives on Frame / Packet.
					 * @param other Source item.
					 * @return *this.
					 */
					Item& operator=(const Item& other) noexcept = default;

					/**
					 * @brief Move assignment.
					 * @param other Item to take.
					 * @return *this.
					 */
					Item& operator=(Item&& other) noexcept = default;

					/**
					 * @}
					 */

				private:
					/**
					 * @brief Allows the decoded-AU holder to access item identity.
					 */
					friend class Backend::Pipeline::Frame;

					/**
					 * @brief Allows the compressed-AU holder to access item identity.
					 */
					friend class Backend::Pipeline::Packet;

					/**
					 * @brief Allows hoppers to clone items internally.
					 */
					friend class Backend::Pipeline::Pipe;

					/**
					 * @brief Allows the decoder to access item identity.
					 */
					friend class Decoder;

					/**
					 * @brief Allows the demuxer to access item identity.
					 */
					friend class Demuxer;

					/**
					 * @brief Allows the encoder to access item identity.
					 */
					friend class Encoder;

					/**
					 * @brief Allows the filter base to access item identity.
					 */
					friend class Filter::FFmpeg;

					/**
					 * @brief Allows the frame facade to construct its item base.
					 */
					friend class Frame;

					/**
					 * @brief Allows the muxer to access item identity.
					 */
					friend class Muxer;

					/**
					 * @brief Allows the packet facade to construct its item base.
					 */
					friend class Packet;

					/**
					 * @brief Allows pipeline stages to access item identity.
					 */
					friend class Step;

					/**
					 * @brief Builds the facade.
					 * @param track Origin container stream index.
					 * @param type Media of this unit. Not Copy.
					 * @param kind Kind::Frame or Kind::Packet.
					 * @param producer Stage that created this unit.
					 */
					Item(int track, enum StormByte::Multimedia::Type type, enum Kind kind, enum Producer producer) noexcept
						: m_track(track), m_type(type), m_kind(kind), m_producer(producer) {}

					int m_track;						///< Origin container stream index, or -1 on the sentinel.
					enum StormByte::Multimedia::Type m_type;	///< Media type, or Unknown on the sentinel.
					enum Kind m_kind;					///< Concrete access-unit kind.
					enum Producer m_producer;				///< Stage that created this unit.
			};
		}
	}
}

/**
 * @brief Registers the completed polymorphic interface for Safe pointer ownership.
 *
 * Identity fields have no heap ownership. The exported virtual destructor dispatches
 * to the dynamic provider; concrete implementations remain responsible for their
 * own members and allocation. Base-heap owners and compatible provider ABIs are
 * required, and the providers must remain loaded until the last owner is released.
 * Protected Clonable inheritance keeps cloning an internal pipeline operation.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Item);

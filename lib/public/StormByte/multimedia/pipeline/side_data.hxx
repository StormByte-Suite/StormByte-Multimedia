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
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/type_traits/safe.hxx>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte suite.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Audio and video processing types.
	 *
	 * @ingroup multimedia_pipeline
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Pipeline
		 * @brief Demux / decode / filter / encode / mux types.
		 *
		 * @ingroup multimedia_pipeline
		 */
		namespace Pipeline {
			/**
			 * @enum SideDataKind
			 * @brief Known libav side-data kinds on a @ref Frame or @ref Packet.
			 *
			 * Unmapped libav types use @ref SideDataKind::Other and keep the
			 * FFmpeg name in @ref SideData::Name.
			 */
			enum class SideDataKind {
				MasteringDisplay,		///< Mastering display metadata.
				ContentLight,			///< MaxCLL / MaxFALL.
				HdrPlus,			///< HDR10+ dynamic metadata.
				HdrVivid,			///< HDR Vivid dynamic metadata.
				A53CC,				///< CEA-708 / A53 captions.
				Stereo3D,			///< Stereo 3D.
				DisplayMatrix,			///< Display matrix.
				IccProfile,			///< ICC profile.
				S12MTimecode,			///< SMPTE ST 12-1 timecode.
				Spherical,			///< Spherical mapping.
				SeiUnregistered,		///< Unregistered SEI.
				FilmGrain,			///< Film grain parameters.
				DolbyVisionRpu,			///< Dolby Vision RPU.
				DolbyVision,			///< Dolby Vision metadata.
				AmbientViewing,			///< Ambient viewing environment.
				Other				///< Unknown kind; Name() is set.
			};

			/**
			 * @class SideData
			 * @brief One side-data blob attached to a Frame or Packet.
			 *
			 * Copies duplicate the side-data bytes independently of the original.
			 *
			 * @ingroup multimedia_pipeline
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC SideData {
				public:
					/**
					 * @brief Empty unnamed blob of kind Other for Safe value storage.
					 * @throws StormByte::Exception If safe optional storage cannot be allocated.
					 */
					SideData();

					/**
					 * @brief Known kind plus payload.
					 * @param kind Side-data kind.
					 * @param payload Raw bytes.
					 * @throws StormByte::Exception If safe optional storage cannot be allocated.
					 */
					SideData(SideDataKind kind, StormByte::Buffer::FIFO payload);

					/**
					 * @brief Other kind plus FFmpeg name and payload.
					 * @param name libav side-data name.
					 * @param payload Raw bytes.
					 *
					 * Sets @ref Kind to @ref SideDataKind::Other.
					 * @throws StormByte::Exception If optional storage cannot be allocated.
					 */
					SideData(StormByte::Safe::String name, StormByte::Buffer::FIFO payload);

					/**
					 * @brief Copy constructor.
					 * @param other Source blob.
					 * @throws StormByte::Exception If safe metadata storage cannot be copied.
					 */
					SideData(const SideData& other);

					/**
					 * @brief Move constructor.
					 * @param other Blob to take.
					 */
					SideData(SideData&& other) noexcept;

					/**
					 * @brief Destructor.
					 */
					~SideData() noexcept;

					/**
					 * @brief Copy assignment.
					 * @param other Source blob.
					 * @return *this.
					 * @throws StormByte::Exception If safe metadata storage cannot be copied.
					 */
					SideData& operator=(const SideData& other);

					/**
					 * @brief Move assignment.
					 * @param other Blob to take.
					 * @return *this.
					 */
					SideData& operator=(SideData&& other) noexcept;

					/**
					 * @brief Kind.
					 * @return Kind.
					 */
					SideDataKind Kind() const noexcept;

					/**
					 * @brief libav name when Kind is Other.
					 * @return Borrowed name, or empty; valid until this blob is modified or destroyed.
					 */
					const StormByte::Safe::Optional<StormByte::Safe::String>& Name() const noexcept;

					/**
					 * @brief Raw payload.
					 * @return FIFO.
					 */
					const StormByte::Buffer::FIFO& Payload() const noexcept;

					/**
					 * @brief Raw payload (mutable).
					 * @return FIFO.
					 */
					StormByte::Buffer::FIFO& Payload() noexcept;

				private:
					SideDataKind m_kind = SideDataKind::Other;			///< Side-data kind; Other for an empty blob.
					StormByte::Safe::Optional<StormByte::Safe::String> m_name;	///< Optional Base-owned name for an unmapped kind.
					StormByte::Buffer::FIFO m_payload;				///< Buffer-owned bytes, copied independently with this value.
			};
		}
	}
}

/**
 * @brief Admits the completed blob to Safe collections under the provider ABI contract.
 *
 * Text and optional storage use Base's heap; FIFO lifetime operations use Buffer's
 * provider. Copy, move and destruction are exported by Multimedia. Consumers must
 * use compatible compiler and runtime ABIs and keep these providers loaded.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::SideData);

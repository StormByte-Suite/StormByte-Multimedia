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
#include <StormByte/multimedia/visibility.h>

#include <optional>
#include <string>

/**
 * @namespace StormByte::Multimedia::Pipeline
 * @brief Demux / decode / filter / encode / mux types.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Pipeline {
	/**
	 * @enum SideDataKind
	 * @brief Known libav side-data kinds on a @ref Frame or @ref Packet.
	 *
	 * Unmapped libav types use @ref SideDataKind::Other and keep the
	 * FFmpeg name in @ref SideData::Name.
	 */
	enum class SideDataKind {
		MasteringDisplay,	///< Mastering display metadata
		ContentLight,		///< MaxCLL / MaxFALL
		HdrPlus,			///< HDR10+ dynamic metadata
		HdrVivid,			///< HDR Vivid dynamic metadata
		A53CC,				///< CEA-708 / A53 captions
		Stereo3D,			///< Stereo 3D
		DisplayMatrix,		///< Display matrix
		IccProfile,			///< ICC profile
		S12MTimecode,		///< SMPTE ST 12-1 timecode
		Spherical,			///< Spherical mapping
		SeiUnregistered,	///< Unregistered SEI
		FilmGrain,			///< Film grain parameters
		DolbyVisionRpu,		///< Dolby Vision RPU
		DolbyVision,		///< Dolby Vision metadata
		AmbientViewing,		///< Ambient viewing environment
		Other				///< Unknown; Name() is set
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
			 * @brief Known kind plus payload.
			 * @param kind Side-data kind.
			 * @param payload Raw bytes.
			 */
			SideData(SideDataKind kind, StormByte::Buffer::FIFO payload) noexcept;

			/**
			 * @brief Other kind plus FFmpeg name and payload.
			 * @param name libav side-data name.
			 * @param payload Raw bytes.
			 *
			 * Sets @ref Kind to @ref SideDataKind::Other.
			 */
			SideData(std::string name, StormByte::Buffer::FIFO payload) noexcept;

			/**
			 * @brief Copy constructor.
			 * @param other Source blob.
			 */
			SideData(const SideData& other) noexcept = default;

			/**
			 * @brief Move constructor.
			 * @param other Blob to take.
			 */
			SideData(SideData&& other) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			~SideData() noexcept = default;

			/**
			 * @brief Copy assignment.
			 * @param other Source blob.
			 * @return *this.
			 */
			SideData& operator=(const SideData& other) noexcept = default;

			/**
			 * @brief Move assignment.
			 * @param other Blob to take.
			 * @return *this.
			 */
			SideData& operator=(SideData&& other) noexcept = default;

			/**
			 * @brief Kind.
			 * @return Kind.
			 */
			SideDataKind Kind() const noexcept;

			/**
			 * @brief libav name when Kind is Other.
			 * @return Name, or empty.
			 */
			const std::optional<std::string>& Name() const noexcept;

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
			SideDataKind m_kind;					///< Kind
			std::optional<std::string> m_name;		///< Name if Other
			StormByte::Buffer::FIFO m_payload;		///< Bytes
	};
}

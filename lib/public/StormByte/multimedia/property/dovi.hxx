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

#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/binary.hxx>

#include <cstdint>
#include <span>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Public media types: codecs, containers, registry and stream kinds.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Property
		 * @brief Media property value types.
		 */
		namespace Property {
			/**
			 * @class DOVI
			 * @brief Dolby Vision configuration, decoded metadata and owned raw RPU bytes.
			 * @note Loaders accept native FFmpeg side data, not serialized configuration records.
			 *       Compatible provider ABIs and live Base and Multimedia providers are required.
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC DOVI final {
				public:
					/**
					 * @brief Constructs absent Dolby Vision properties.
					 */
					DOVI() noexcept;

					/**
					 * @brief Copies Dolby Vision properties and owned RPU bytes.
					 * @param other Properties to copy.
					 * @throws StormByte::Exception Safe storage copying failed.
					 */
					DOVI(const DOVI& other);

					/**
					 * @brief Moves Dolby Vision properties.
					 * @param other Properties to move.
					 */
					DOVI(DOVI&& other) noexcept;

					/**
					 * @brief Releases owned RPU storage through its provider.
					 */
					~DOVI() noexcept;

					/**
					 * @brief Copies Dolby Vision properties.
					 * @param other Properties to copy.
					 * @return This object.
					 * @throws StormByte::Exception Safe storage copying failed.
					 */
					DOVI& operator=(const DOVI& other);

					/**
					 * @brief Moves Dolby Vision properties.
					 * @param other Properties to move.
					 * @return This object.
					 */
					DOVI& operator=(DOVI&& other) noexcept;

					/**
					 * @brief Loads native AVDOVIDecoderConfigurationRecord side data.
					 * @param bytes Native configuration bytes.
					 * @return True on success; truncated input leaves properties unchanged.
					 */
					bool LoadConfiguration(std::span<const std::byte> bytes) noexcept;

					/**
					 * @brief Loads native AVDOVIMetadata after checking its dynamic offsets.
					 * @param bytes Native decoded metadata bytes.
					 * @return True on success; invalid ranges leave properties unchanged.
					 * @note Stream configuration profile and level are never replaced by RPU identifiers.
					 */
					bool LoadMetadata(std::span<const std::byte> bytes) noexcept;

					/**
					 * @brief Copies raw RPU side data into owned Safe binary storage.
					 * @param bytes Raw RPU bytes.
					 * @return True on success; empty input leaves properties unchanged.
					 * @throws StormByte::Exception Safe storage allocation failed.
					 */
					bool LoadRpu(std::span<const std::byte> bytes);

					/**
					 * @brief Reports Dolby Vision detected by any loader.
					 * @return Whether configuration, parsed metadata or raw RPU is present.
					 */
					bool Present() const noexcept;

					/**
					 * @brief Reports native stream configuration availability.
					 * @return Whether configuration was loaded.
					 */
					bool ConfigurationPresent() const noexcept;

					/**
					 * @brief Reports decoded metadata availability.
					 * @return Whether parsed fields are available.
					 */
					bool MetadataPresent() const noexcept;

					/**
					 * @brief Returns the stream configuration profile.
					 * @return Profile, or zero without configuration.
					 */
					std::uint8_t Profile() const noexcept;

					/**
					 * @brief Returns the stream configuration level.
					 * @return Level, or zero without configuration.
					 */
					std::uint8_t Level() const noexcept;

					/**
					 * @brief Returns the configuration major version.
					 * @return Major version, or zero without configuration.
					 */
					std::uint8_t VersionMajor() const noexcept;

					/**
					 * @brief Returns the configuration minor version.
					 * @return Minor version, or zero without configuration.
					 */
					std::uint8_t VersionMinor() const noexcept;

					/**
					 * @brief Returns base-layer signal compatibility.
					 * @return Native compatibility identifier, or zero without configuration.
					 */
					std::uint8_t CompatibilityId() const noexcept;

					/**
					 * @brief Reports signaled or decoded RPU presence.
					 * @return Whether an RPU is signaled or was loaded.
					 */
					bool RpuPresent() const noexcept;

					/**
					 * @brief Reports configuration enhancement-layer presence.
					 * @return Enhancement-layer flag, or false without configuration.
					 */
					bool ElPresent() const noexcept;

					/**
					 * @brief Reports configuration base-layer presence.
					 * @return Base-layer flag, or false without configuration.
					 */
					bool BlPresent() const noexcept;

					/**
					 * @brief Returns the metadata compression method.
					 * @return Native compression identifier, or zero without configuration.
					 */
					std::uint8_t Compression() const noexcept;

					/**
					 * @brief Returns owned raw RPU bytes.
					 * @return Borrowed binary valid while this object is alive; empty if unavailable.
					 */
					const StormByte::Safe::Binary& Rpu() const noexcept;

					/**
					 * @brief Returns decoded base-layer bit depth.
					 * @return Bit depth, or zero without metadata.
					 */
					std::uint8_t BlBitDepth() const noexcept;

					/**
					 * @brief Returns decoded enhancement-layer bit depth.
					 * @return Bit depth, or zero without metadata.
					 */
					std::uint8_t ElBitDepth() const noexcept;

					/**
					 * @brief Returns decoded VDR bit depth.
					 * @return Bit depth, or zero without metadata.
					 */
					std::uint8_t VdrBitDepth() const noexcept;

					/**
					 * @brief Returns decoded mapping color space.
					 * @return Native mapping color-space identifier, or zero without metadata.
					 */
					std::uint8_t MappingColorSpace() const noexcept;

					/**
					 * @brief Returns decoded mapping chroma format.
					 * @return Native mapping chroma-format identifier, or zero without metadata.
					 */
					std::uint8_t MappingChromaFormat() const noexcept;

					/**
					 * @brief Returns decoded signal bit depth.
					 * @return Bit depth, or zero without metadata.
					 */
					std::uint8_t SignalBitDepth() const noexcept;

					/**
					 * @brief Returns decoded signal color space.
					 * @return Native signal color-space identifier, or zero without metadata.
					 */
					std::uint8_t SignalColorSpace() const noexcept;

					/**
					 * @brief Returns decoded signal chroma format.
					 * @return Native signal chroma-format identifier, or zero without metadata.
					 */
					std::uint8_t SignalChromaFormat() const noexcept;

					/**
					 * @brief Returns decoded signal range flags.
					 * @return Native range flags, or zero without metadata.
					 */
					std::uint8_t SignalFullRange() const noexcept;

					/**
					 * @brief Returns decoded signal EOTF.
					 * @return Native EOTF identifier, or zero without metadata.
					 */
					std::uint16_t SignalEotf() const noexcept;

					/**
					 * @brief Returns decoded source minimum PQ code value.
					 * @return Minimum PQ, or zero without metadata.
					 */
					std::uint16_t SourceMinPq() const noexcept;

					/**
					 * @brief Returns decoded source maximum PQ code value.
					 * @return Maximum PQ, or zero without metadata.
					 */
					std::uint16_t SourceMaxPq() const noexcept;

				private:
					bool m_configuration = false;		///< Native configuration availability.
					bool m_metadata = false;				///< Decoded metadata availability.
					std::uint8_t m_version_major = 0;		///< Configuration major version.
					std::uint8_t m_version_minor = 0;		///< Configuration minor version.
					std::uint8_t m_profile = 0;			///< Stream configuration profile.
					std::uint8_t m_level = 0;				///< Stream configuration level.
					std::uint8_t m_compatibility = 0;		///< Base-layer compatibility identifier.
					bool m_rpu_present = false;			///< Configuration RPU presence flag.
					bool m_el_present = false;			///< Configuration enhancement-layer flag.
					bool m_bl_present = false;			///< Configuration base-layer flag.
					std::uint8_t m_compression = 0;		///< Metadata compression identifier.
					StormByte::Safe::Binary m_rpu;		///< Owned raw RPU bytes.
					std::uint8_t m_bl_bit_depth = 0;		///< Decoded base-layer bit depth.
					std::uint8_t m_el_bit_depth = 0;		///< Decoded enhancement-layer bit depth.
					std::uint8_t m_vdr_bit_depth = 0;		///< Decoded VDR bit depth.
					std::uint8_t m_mapping_color_space = 0;	///< Decoded mapping color space.
					std::uint8_t m_mapping_chroma_format = 0;	///< Decoded mapping chroma format.
					std::uint8_t m_signal_bit_depth = 0;	///< Decoded signal bit depth.
					std::uint8_t m_signal_color_space = 0;	///< Decoded signal color space.
					std::uint8_t m_signal_chroma_format = 0;	///< Decoded signal chroma format.
					std::uint8_t m_signal_full_range = 0;	///< Decoded signal range flags.
					std::uint16_t m_signal_eotf = 0;		///< Decoded signal EOTF identifier.
					std::uint16_t m_source_min_pq = 0;		///< Decoded minimum PQ code value.
					std::uint16_t m_source_max_pq = 0;		///< Decoded maximum PQ code value.
			};
		}
	}
}

/**
 * @brief Registers Dolby Vision properties with provider-owned RPU storage.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Property::DOVI);

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

#include <StormByte/multimedia/property/dovi.hxx>

#include <cstring>

extern "C" {
	#include <libavutil/dovi_meta.h>
}

using namespace StormByte::Multimedia::Property;

DOVI::DOVI() noexcept = default;

DOVI::DOVI(const DOVI& other) = default;

DOVI::DOVI(DOVI&& other) noexcept = default;

DOVI::~DOVI() noexcept = default;

DOVI& DOVI::operator=(const DOVI& other) = default;

DOVI& DOVI::operator=(DOVI&& other) noexcept = default;

bool DOVI::LoadConfiguration(std::span<const std::byte> bytes) noexcept {
	if (bytes.size() < sizeof(AVDOVIDecoderConfigurationRecord))
		return false;
	AVDOVIDecoderConfigurationRecord configuration;
	std::memcpy(&configuration, bytes.data(), sizeof(configuration));
	m_version_major = configuration.dv_version_major;
	m_version_minor = configuration.dv_version_minor;
	m_profile = configuration.dv_profile;
	m_level = configuration.dv_level;
	m_compatibility = configuration.dv_bl_signal_compatibility_id;
	m_rpu_present = configuration.rpu_present_flag != 0;
	m_el_present = configuration.el_present_flag != 0;
	m_bl_present = configuration.bl_present_flag != 0;
	m_compression = configuration.dv_md_compression;
	m_configuration = true;
	return true;
}

bool DOVI::LoadMetadata(std::span<const std::byte> bytes) noexcept {
	if (bytes.size() < sizeof(AVDOVIMetadata))
		return false;
	AVDOVIMetadata metadata;
	std::memcpy(&metadata, bytes.data(), sizeof(metadata));
	const auto validRange = [bytes](std::size_t offset, std::size_t size) noexcept {
		return offset >= sizeof(AVDOVIMetadata) && offset <= bytes.size()
			&& size <= bytes.size() - offset;
	};
	if (!validRange(metadata.header_offset, sizeof(AVDOVIRpuDataHeader))
		|| !validRange(metadata.mapping_offset, sizeof(AVDOVIDataMapping))
		|| !validRange(metadata.color_offset, sizeof(AVDOVIColorMetadata)))
		return false;
	if (metadata.num_ext_blocks < 0 || metadata.num_ext_blocks > AV_DOVI_MAX_EXT_BLOCKS)
		return false;
	if (metadata.num_ext_blocks > 0 && (metadata.ext_block_size < sizeof(AVDOVIDmData)
		|| !validRange(metadata.ext_block_offset, 0)
		|| static_cast<std::size_t>(metadata.num_ext_blocks)
			> (bytes.size() - metadata.ext_block_offset) / metadata.ext_block_size))
		return false;
	AVDOVIRpuDataHeader header;
	AVDOVIDataMapping mapping;
	AVDOVIColorMetadata color;
	std::memcpy(&header, bytes.data() + metadata.header_offset, sizeof(header));
	std::memcpy(&mapping, bytes.data() + metadata.mapping_offset, sizeof(mapping));
	std::memcpy(&color, bytes.data() + metadata.color_offset, sizeof(color));
	m_bl_bit_depth = header.bl_bit_depth;
	m_el_bit_depth = header.el_bit_depth;
	m_vdr_bit_depth = header.vdr_bit_depth;
	m_mapping_color_space = mapping.mapping_color_space;
	m_mapping_chroma_format = mapping.mapping_chroma_format_idc;
	m_signal_bit_depth = color.signal_bit_depth;
	m_signal_color_space = color.signal_color_space;
	m_signal_chroma_format = color.signal_chroma_format;
	m_signal_full_range = color.signal_full_range_flag;
	m_signal_eotf = color.signal_eotf;
	m_source_min_pq = color.source_min_pq;
	m_source_max_pq = color.source_max_pq;
	m_metadata = true;
	return true;
}

bool DOVI::LoadRpu(std::span<const std::byte> bytes) {
	if (bytes.empty())
		return false;
	m_rpu = StormByte::Safe::Binary(bytes);
	return true;
}

bool DOVI::Present() const noexcept {
	return m_configuration || RpuPresent();
}

bool DOVI::ConfigurationPresent() const noexcept {
	return m_configuration;
}

bool DOVI::MetadataPresent() const noexcept {
	return m_metadata;
}

std::uint8_t DOVI::Profile() const noexcept {
	return m_profile;
}

std::uint8_t DOVI::Level() const noexcept {
	return m_level;
}

std::uint8_t DOVI::VersionMajor() const noexcept {
	return m_version_major;
}

std::uint8_t DOVI::VersionMinor() const noexcept {
	return m_version_minor;
}

std::uint8_t DOVI::CompatibilityId() const noexcept {
	return m_compatibility;
}

bool DOVI::RpuPresent() const noexcept {
	return m_rpu_present || m_metadata || !m_rpu.empty();
}

bool DOVI::ElPresent() const noexcept {
	return m_el_present;
}

bool DOVI::BlPresent() const noexcept {
	return m_bl_present;
}

std::uint8_t DOVI::Compression() const noexcept {
	return m_compression;
}

const StormByte::Safe::Binary& DOVI::Rpu() const noexcept {
	return m_rpu;
}

std::uint8_t DOVI::BlBitDepth() const noexcept {
	return m_bl_bit_depth;
}

std::uint8_t DOVI::ElBitDepth() const noexcept {
	return m_el_bit_depth;
}

std::uint8_t DOVI::VdrBitDepth() const noexcept {
	return m_vdr_bit_depth;
}

std::uint8_t DOVI::MappingColorSpace() const noexcept {
	return m_mapping_color_space;
}

std::uint8_t DOVI::MappingChromaFormat() const noexcept {
	return m_mapping_chroma_format;
}

std::uint8_t DOVI::SignalBitDepth() const noexcept {
	return m_signal_bit_depth;
}

std::uint8_t DOVI::SignalColorSpace() const noexcept {
	return m_signal_color_space;
}

std::uint8_t DOVI::SignalChromaFormat() const noexcept {
	return m_signal_chroma_format;
}

std::uint8_t DOVI::SignalFullRange() const noexcept {
	return m_signal_full_range;
}

std::uint16_t DOVI::SignalEotf() const noexcept {
	return m_signal_eotf;
}

std::uint16_t DOVI::SourceMinPq() const noexcept {
	return m_source_min_pq;
}

std::uint16_t DOVI::SourceMaxPq() const noexcept {
	return m_source_max_pq;
}

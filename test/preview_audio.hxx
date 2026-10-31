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

#include <StormByte/exception.hxx>
#include <StormByte/multimedia/display.hxx>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

/**
 * @namespace PreviewAdapter
 * @brief Testable demo-only audio mapping; not part of the installed API.
 */
namespace PreviewAdapter {
	/**
	 * @brief Describe decoded PCM without relying on enumeration ordinals.
	 * @param format Decoded representation.
	 * @return Stable human-readable sample name.
	 */
	inline const char* SampleName(StormByte::Multimedia::AudioSample format) {
		using StormByte::Multimedia::AudioSample;
		switch (format) {
			case AudioSample::Unsigned8: return "u8";
			case AudioSample::Signed16: return "s16";
			case AudioSample::Signed32: return "s32";
			case AudioSample::Signed64: return "s64";
			case AudioSample::Float32: return "f32";
			case AudioSample::Float64: return "f64";
		}
		return "unsupported";
	}

	/**
	 * @brief Return the exact decoded scalar width.
	 * @param format Decoded PCM representation.
	 * @return Bytes per scalar.
	 * @throws StormByte::Exception If the representation is unsupported.
	 */
	inline std::size_t SampleBytes(StormByte::Multimedia::AudioSample format) {
		using StormByte::Multimedia::AudioSample;
		switch (format) {
			case AudioSample::Unsigned8: return 1;
			case AudioSample::Signed16: return 2;
			case AudioSample::Signed32:
			case AudioSample::Float32: return 4;
			case AudioSample::Signed64:
			case AudioSample::Float64: return 8;
		}
		throw StormByte::Exception("unsupported decoded sample format");
	}

	/**
	 * @brief Map an exact speaker set into SDL2's documented channel order.
	 * @param audio Source metadata.
	 * @return SDL channel to source channel indices, retaining every channel.
	 * @throws StormByte::Exception If the exact layout cannot be represented.
	 */
	inline std::vector<std::size_t> ChannelMap(const StormByte::Multimedia::PreviewAudio& audio) {
		using StormByte::Multimedia::AudioLayout;
		if (audio.m_layout == AudioLayout::Unspecified || audio.m_layout == AudioLayout::Ambisonic
			|| !audio.m_rate || audio.m_rate > 384000)
			throw StormByte::Exception("SDL demo cannot map unspecified or ambisonic audio layouts");
		std::vector<std::string_view> order;
		switch (static_cast<std::size_t>(audio.m_channels.size())) {
			case 1: order = {"FC"}; break;
			case 2: order = {"FL", "FR"}; break;
			case 3: order = {"FL", "FR", "LFE"}; break;
			case 4: order = {"FL", "FR", "BL", "BR"}; break;
			case 5: order = {"FL", "FR", "LFE", "BL", "BR"}; break;
			case 6: {
				const bool side = std::find(audio.m_channels.begin(), audio.m_channels.end(), StormByte::Safe::String("SL"))
					!= audio.m_channels.end();
				order = {"FL", "FR", "FC", "LFE", side ? "SL" : "BL", side ? "SR" : "BR"};
				break;
			}
			case 7: order = {"FL", "FR", "FC", "LFE", "BC", "SL", "SR"}; break;
			case 8: order = {"FL", "FR", "FC", "LFE", "BL", "BR", "SL", "SR"}; break;
			default: throw StormByte::Exception("SDL demo does not support this channel count");
		}
		std::vector<std::size_t> mapping;
		for (const auto identity : order) {
			std::size_t index = 0;
			while (index < audio.m_channels.size() && std::string_view(audio.m_channels[index]) != identity)
				++index;
			if (index == audio.m_channels.size()
				|| std::count(audio.m_channels.begin(), audio.m_channels.end(), StormByte::Safe::String(identity)) != 1)
				throw StormByte::Exception("SDL demo cannot map this exact speaker layout");
			mapping.push_back(index);
		}
		return mapping;
	}

	/**
	 * @brief Interleave/reorder native bytes for SDL; convert only unsupported wide scalars.
	 * @param audio Exact decoded PCM.
	 * @param mapping Validated SDL channel to source channel indices.
	 * @return Native packed PCM, or float32 for signed64/float64 sources.
	 * @throws StormByte::Exception If storage or metadata is inconsistent or oversized.
	 */
	inline std::vector<std::uint8_t> Pack(const StormByte::Multimedia::PreviewAudio& audio,
		const std::vector<std::size_t>& mapping) {
		using StormByte::Multimedia::AudioSample;
		const auto channels = static_cast<std::size_t>(audio.m_channels.size());
		const auto width = SampleBytes(audio.m_format);
		if (!channels || channels > 8 || mapping.size() != channels || !audio.m_rate
			|| audio.m_rate > 384000 || audio.m_frames > audio.m_rate
			|| !audio.m_frames || audio.m_frames > 16 * 1024 * 1024 / width / channels
			|| audio.m_planes.size() != (audio.m_planar ? channels : 1))
			throw StormByte::Exception("invalid or oversized decoded audio block");
		for (const auto source : mapping)
			if (source >= channels)
				throw StormByte::Exception("invalid audio channel index");
		for (const auto& plane : audio.m_planes)
			if (static_cast<std::size_t>(plane.size()) != audio.m_frames * width * (audio.m_planar ? 1 : channels))
				throw StormByte::Exception("invalid decoded audio plane size");
		const bool wide = audio.m_format == AudioSample::Float64 || audio.m_format == AudioSample::Signed64;
		const auto output_width = wide ? sizeof(float) : width;
		std::vector<std::uint8_t> packed(static_cast<std::size_t>(audio.m_frames) * channels * output_width);
		for (std::size_t frame = 0; frame < audio.m_frames; ++frame)
			for (std::size_t channel = 0; channel < channels; ++channel) {
				const auto source = mapping[channel];
				const auto& plane = audio.m_planes[audio.m_planar ? source : 0];
				const auto offset = (audio.m_planar ? frame : frame * channels + source) * width;
				auto* destination = packed.data() + (frame * channels + channel) * output_width;
				if (wide) {
					float value;
					if (audio.m_format == AudioSample::Float64) {
						double decoded;
						std::memcpy(&decoded, plane.data() + offset, sizeof(decoded));
						value = static_cast<float>(decoded);
					}
					else {
						std::int64_t decoded;
						std::memcpy(&decoded, plane.data() + offset, sizeof(decoded));
						value = static_cast<float>(static_cast<double>(decoded) / 9223372036854775808.0);
					}
					std::memcpy(destination, &value, sizeof(value));
				}
				else
					std::memcpy(destination, plane.data() + offset, width);
			}
		return packed;
	}
}

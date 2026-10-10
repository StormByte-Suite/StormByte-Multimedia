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

#include <cstdint>

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
		 * @enum Feature
		 * @brief Handcrafted capability of one decoder or encoder implementation.
		 *
		 * Preference is not a Feature; it lives on the table row.
		 * Bits must be disjoint: Has() is a real bitmask test.
		 * HDR10 / HDR10Plus / SideData mean the implementation surfaces
		 * that metadata on Pipeline::Frame (decode) or writes it (encode).
		 * DOVI means the bundled FFmpeg implementation emits parsed
		 * AV_FRAME_DATA_DOVI_METADATA (decode), or has a native Dolby Vision
		 * configure/generate path consuming that metadata (encode).
		 * Raw RPU forwarding, container configuration and hardware acceleration
		 * alone do not qualify. This is not a profile, rendering, transform-validity
		 * or Dolby certification guarantee, nor automatic pipeline negotiation.
		 * @note DOVI requires supported input/profile, codec options and dependencies.
		 * Metadata retention does not prove semantic validity after pixel changes.
		 */
		enum class Feature: std::uint32_t {
			None					= 0,			///< No flags.
			MultiThreaded			= 1u << 0,		///< Frame or slice threading.
			RealTime				= 1u << 1,		///< Suitable for real-time use.
			LowDelay				= 1u << 2,		///< Low latency.
			ZeroCopy				= 1u << 3,		///< Zero-copy paths.
			HardwareAcceleration	= 1u << 4,		///< Hardware decode or encode.
			HighQuality				= 1u << 5,		///< High quality presets.
			PsychoVisual			= 1u << 6,		///< Psycho-visual tuning.
			AdaptiveQuantization	= 1u << 7,		///< Adaptive quantization.
			Lookahead				= 1u << 8,		///< Lookahead analysis.
			TwoPass					= 1u << 9,		///< Two-pass encoding.
			Lossless				= 1u << 10,		///< Lossless mode.
			ProfileBased			= 1u << 11,		///< Profile selection.
			ContentTuning			= 1u << 12,		///< Content-type tuning.
			TenBit					= 1u << 13,		///< 10-bit.
			TwelveBit				= 1u << 14,		///< 12-bit.
			HDR10					= 1u << 15,		///< Static HDR10 metadata on the frame.
			HDR10Plus				= 1u << 16,		///< Dynamic HDR10+ side data on the frame.
			WideGamut				= 1u << 17,		///< Wide color gamut.
			SurroundSound			= 1u << 18,		///< Surround layouts.
			SideData				= 1u << 19,		///< Propagates packet or frame side data.
			BFrames					= 1u << 20,		///< B-frames.
			IntraOnly				= 1u << 21,		///< Intra-only.
			Interlaced				= 1u << 22,		///< Interlaced.
			Slices					= 1u << 23,		///< Slice coding.
			Streamable				= 1u << 24,		///< Streamable output.
			Encodeable				= 1u << 25,		///< Can encode (legacy; Write is Codec::HasAccess).
			DOVI					= 1u << 26		///< Parsed Dolby Vision metadata decoding or native RPU encoding.
		};

		/**
		 * @brief Converts a Feature to a stable token.
		 * @param feature Feature value.
		 * @return Literal. Unknown bits return "UnknownFeature".
		 */
		constexpr const char* ToString(Feature feature) noexcept {
			switch (feature) {
				case Feature::None:					return "None";
				case Feature::MultiThreaded:			return "MultiThreaded";
				case Feature::RealTime:				return "RealTime";
				case Feature::LowDelay:				return "LowDelay";
				case Feature::ZeroCopy:				return "ZeroCopy";
				case Feature::HardwareAcceleration:	return "HardwareAcceleration";
				case Feature::HighQuality:			return "HighQuality";
				case Feature::PsychoVisual:			return "PsychoVisual";
				case Feature::AdaptiveQuantization:	return "AdaptiveQuantization";
				case Feature::Lookahead:				return "Lookahead";
				case Feature::TwoPass:				return "TwoPass";
				case Feature::Lossless:				return "Lossless";
				case Feature::ProfileBased:			return "ProfileBased";
				case Feature::ContentTuning:			return "ContentTuning";
				case Feature::TenBit:					return "TenBit";
				case Feature::TwelveBit:				return "TwelveBit";
				case Feature::HDR10:					return "HDR10";
				case Feature::HDR10Plus:				return "HDR10Plus";
				case Feature::DOVI:					return "DOVI";
				case Feature::WideGamut:				return "WideGamut";
				case Feature::SurroundSound:			return "SurroundSound";
				case Feature::SideData:				return "SideData";
				case Feature::BFrames:					return "BFrames";
				case Feature::IntraOnly:				return "IntraOnly";
				case Feature::Interlaced:				return "Interlaced";
				case Feature::Slices:					return "Slices";
				case Feature::Streamable:			return "Streamable";
				case Feature::Encodeable:			return "Encodeable";
			}
			return "UnknownFeature";
		}
	}
}

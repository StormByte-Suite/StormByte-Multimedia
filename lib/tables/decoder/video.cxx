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

#include <tables/decoder/table.hxx>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Tables::Decoder;

namespace {
	constexpr DecoderDef table[] = {
		{ "H.264", "h264", "Native H.264 decoder", 0, Feature::BFrames | Feature::Slices | Feature::IntraOnly | Feature::Interlaced | Feature::SideData },
		{ "H.264", "h264_cuvid", "NVIDIA CUVID H.264", 50, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::IntraOnly | Feature::ZeroCopy },
		{ "H.264", "h264_nvdec", "NVIDIA NVDEC H.264", 51, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::IntraOnly | Feature::ZeroCopy },
		{ "H.264", "h264_qsv", "Intel QSV H.264", 52, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::IntraOnly },
		{ "H.264", "h264_vaapi", "VAAPI H.264", 53, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices },
		{ "H.264", "h264_videotoolbox", "VideoToolbox H.264", 54, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices },
		{ "H.265", "hevc", "Native HEVC decoder", 0, Feature::BFrames | Feature::Slices | Feature::IntraOnly | Feature::Interlaced | Feature::TenBit | Feature::TwelveBit | Feature::HDR10 | Feature::HDR10Plus | Feature::DOVI | Feature::WideGamut | Feature::SideData },
		{ "H.265", "hevc_cuvid", "NVIDIA CUVID HEVC", 50, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::TenBit | Feature::HDR10 | Feature::WideGamut | Feature::ZeroCopy },
		{ "H.265", "hevc_nvdec", "NVIDIA NVDEC HEVC", 51, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::TenBit | Feature::HDR10 | Feature::WideGamut | Feature::ZeroCopy },
		{ "H.265", "hevc_qsv", "Intel QSV HEVC", 52, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::TenBit | Feature::HDR10 },
		{ "H.265", "hevc_vaapi", "VAAPI HEVC", 53, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::TenBit | Feature::HDR10 },
		{ "H.265", "hevc_videotoolbox", "VideoToolbox HEVC", 54, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::TenBit },
		{ "AV1", "av1", "Native AV1 decoder", 0, Feature::TenBit | Feature::TwelveBit | Feature::HDR10 | Feature::HDR10Plus | Feature::DOVI | Feature::WideGamut | Feature::SideData },
		{ "AV1", "libdav1d", "dav1d AV1 decoder", 1, Feature::HighQuality | Feature::TenBit | Feature::TwelveBit | Feature::HDR10 | Feature::HDR10Plus | Feature::DOVI | Feature::WideGamut | Feature::SideData },
		{ "AV1", "av1_cuvid", "NVIDIA CUVID AV1", 50, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::TenBit | Feature::HDR10 | Feature::WideGamut | Feature::ZeroCopy },
		{ "AV1", "av1_qsv", "Intel QSV AV1", 52, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::TenBit | Feature::HDR10 | Feature::WideGamut },
		{ "AV1", "av1_vaapi", "VAAPI AV1", 53, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::TenBit | Feature::HDR10 | Feature::WideGamut },
		{ "VP8", "libvpx", "libvpx VP8 decoder", 0, Feature::LowDelay | Feature::Slices | Feature::SideData },
		{ "VP9", "libvpx-vp9", "libvpx VP9 decoder", 0, Feature::LowDelay | Feature::Slices | Feature::TenBit | Feature::TwelveBit | Feature::HDR10 | Feature::WideGamut | Feature::SideData },
		{ "VP9", "vp9", "Native VP9 decoder", 1, Feature::TenBit | Feature::TwelveBit | Feature::HDR10 | Feature::WideGamut | Feature::SideData },
		{ "VP9", "vp9_cuvid", "NVIDIA CUVID VP9", 50, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::TenBit | Feature::HDR10 | Feature::WideGamut | Feature::ZeroCopy },
		{ "VP9", "vp9_qsv", "Intel QSV VP9", 52, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::TenBit | Feature::HDR10 },
		{ "VP9", "vp9_vaapi", "VAAPI VP9", 53, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::TenBit | Feature::HDR10 | Feature::WideGamut },
	};
}

std::span<const DecoderDef> StormByte::Multimedia::Tables::Decoder::Video() noexcept {
	return table;
}

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

#include <tables/encoder/table.hxx>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Tables::Encoder;

namespace {
	constexpr EncoderDef table[] = {
		{ "H.264", "libx264", "x264 software encoder", 0, Feature::HighQuality | Feature::PsychoVisual | Feature::Lookahead | Feature::TwoPass | Feature::BFrames | Feature::Slices | Feature::ProfileBased | Feature::ContentTuning | Feature::MultiThreaded, "x264-params", "", "", "crf", "b", "maxrate", "bufsize", "preset", "tune" },
		{ "H.264", "libopenh264", "OpenH264 software encoder", 1, Feature::Slices | Feature::ProfileBased | Feature::MultiThreaded, "", "", "", "", "b", "", "", "", "" },
		{ "H.264", "h264_nvenc", "NVIDIA NVENC H.264", 10, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::TenBit | Feature::ProfileBased, "", "", "", "cq", "b", "maxrate", "bufsize", "preset", "" },
		{ "H.264", "h264_qsv", "Intel QSV H.264", 11, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::ProfileBased, "", "", "", "global_quality", "b", "maxrate", "bufsize", "preset", "" },
		{ "H.264", "h264_vaapi", "VAAPI H.264", 12, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::ProfileBased, "", "", "", "qp", "b", "maxrate", "bufsize", "", "" },
		{ "H.264", "h264_videotoolbox", "VideoToolbox H.264", 13, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::ProfileBased, "", "", "", "", "b", "", "", "", "" },
		{ "H.265", "libx265", "x265 software encoder", 0, Feature::HighQuality | Feature::PsychoVisual | Feature::Lookahead | Feature::TwoPass | Feature::BFrames | Feature::Slices | Feature::TenBit | Feature::TwelveBit | Feature::HDR10 | Feature::HDR10Plus | Feature::DOVI | Feature::WideGamut | Feature::ProfileBased | Feature::ContentTuning | Feature::SideData | Feature::MultiThreaded, "x265-params", "hdr10=1", "", "crf", "b", "maxrate", "bufsize", "preset", "tune" },
		{ "H.265", "libkvazaar", "Kvazaar software encoder", 1, Feature::MultiThreaded, "", "", "", "", "b", "", "", "", "" },
		{ "H.265", "hevc_nvenc", "NVIDIA NVENC HEVC", 10, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::TenBit | Feature::HDR10 | Feature::WideGamut | Feature::ProfileBased, "", "", "", "cq", "b", "maxrate", "bufsize", "preset", "" },
		{ "H.265", "hevc_qsv", "Intel QSV HEVC", 11, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::TenBit | Feature::HDR10 | Feature::ProfileBased, "", "", "", "global_quality", "b", "maxrate", "bufsize", "preset", "" },
		{ "H.265", "hevc_vaapi", "VAAPI HEVC", 12, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::TenBit | Feature::HDR10 | Feature::WideGamut, "", "", "", "qp", "b", "maxrate", "bufsize", "", "" },
		{ "H.265", "hevc_videotoolbox", "VideoToolbox HEVC", 13, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::BFrames | Feature::Slices | Feature::TenBit | Feature::ProfileBased, "", "", "", "", "b", "", "", "", "" },
		{ "AV1", "libsvtav1", "SVT-AV1 software encoder", 0, Feature::HighQuality | Feature::PsychoVisual | Feature::Lookahead | Feature::TwoPass | Feature::TenBit | Feature::DOVI | Feature::WideGamut | Feature::ProfileBased | Feature::MultiThreaded, "svtav1-params", "", "", "crf", "b", "", "", "preset", "" },
		{ "AV1", "libaom-av1", "libaom AV1 encoder", 1, Feature::HighQuality | Feature::TwoPass | Feature::TenBit | Feature::TwelveBit | Feature::DOVI | Feature::WideGamut | Feature::ProfileBased, "", "", "", "crf", "b", "", "", "", "" },
		{ "AV1", "av1_nvenc", "NVIDIA NVENC AV1", 10, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::TenBit | Feature::WideGamut | Feature::ProfileBased, "", "", "", "cq", "b", "maxrate", "bufsize", "preset", "" },
		{ "AV1", "av1_qsv", "Intel QSV AV1", 11, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::TenBit | Feature::WideGamut | Feature::ProfileBased, "", "", "", "global_quality", "b", "maxrate", "bufsize", "preset", "" },
		{ "AV1", "av1_vaapi", "VAAPI AV1", 12, Feature::HardwareAcceleration | Feature::LowDelay | Feature::RealTime | Feature::TenBit | Feature::WideGamut, "", "", "", "qp", "b", "maxrate", "bufsize", "", "" },
		{ "VP8", "libvpx", "libvpx VP8 encoder", 0, Feature::LowDelay | Feature::RealTime | Feature::Slices, "", "", "", "crf", "b", "maxrate", "bufsize", "", "" },
		{ "VP9", "libvpx-vp9", "libvpx VP9 encoder", 0, Feature::LowDelay | Feature::RealTime | Feature::Slices | Feature::TenBit | Feature::TwelveBit | Feature::HDR10 | Feature::WideGamut, "", "", "", "crf", "b", "maxrate", "bufsize", "", "" },
	};
}

std::span<const EncoderDef> StormByte::Multimedia::Tables::Encoder::Video() noexcept {
	return table;
}

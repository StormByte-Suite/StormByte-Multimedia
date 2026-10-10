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

#include <StormByte/multimedia/backend/pipeline/detail/content/video.hxx>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hdr_dynamic_metadata.h>
#include <libavutil/rational.h>
}

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>

using StormByte::Multimedia::Backend::Pipeline::Detail::Content::Video;

namespace {
	struct Card {
		int width = 0;
		int height = 0;
		int crop_top = 0;
		int crop_bottom = 0;
		int crop_left = 0;
		int crop_right = 0;
	};

	enum class Kind : unsigned char {
		Identity,
		Scale,
		Crop,
		Pad,
		Unknown
	};

	Card Read(const ::AVFrame* raw) noexcept {
		Card card;
		if (!raw)
			return card;
		card.width = raw->width;
		card.height = raw->height;
		card.crop_top = static_cast<int>(raw->crop_top);
		card.crop_bottom = static_cast<int>(raw->crop_bottom);
		card.crop_left = static_cast<int>(raw->crop_left);
		card.crop_right = static_cast<int>(raw->crop_right);
		return card;
	}

	bool Same(const Card& a, const Card& b) noexcept {
		return a.width == b.width && a.height == b.height &&
			a.crop_top == b.crop_top && a.crop_bottom == b.crop_bottom &&
			a.crop_left == b.crop_left && a.crop_right == b.crop_right;
	}

	Kind Classify(const Card& in, const Card& out) noexcept {
		if (Same(in, out))
			return Kind::Identity;
		if (in.width <= 0 || in.height <= 0 || out.width <= 0 || out.height <= 0)
			return Kind::Unknown;

		const bool crop_same =
			in.crop_top == out.crop_top && in.crop_bottom == out.crop_bottom &&
			in.crop_left == out.crop_left && in.crop_right == out.crop_right;

		if (crop_same) {
			const std::int64_t cross_a = static_cast<std::int64_t>(out.width) * in.height;
			const std::int64_t cross_b = static_cast<std::int64_t>(out.height) * in.width;
			if (cross_a == cross_b)
				return Kind::Scale;
			const bool shrink = out.width <= in.width && out.height <= in.height &&
				(out.width < in.width || out.height < in.height);
			const bool grow = out.width >= in.width && out.height >= in.height &&
				(out.width > in.width || out.height > in.height);
			if (shrink)
				return Kind::Crop;
			if (grow)
				return Kind::Pad;
			return Kind::Unknown;
		}

		if (in.width == out.width && in.height == out.height)
			return Kind::Crop;
		return Kind::Unknown;
	}

	double Rat(const AVRational& r) noexcept {
		if (r.den == 0)
			return 0;
		return static_cast<double>(r.num) / static_cast<double>(r.den);
	}

	AVRational MakeRat(double v) noexcept {
		if (!std::isfinite(v) || v <= 0)
			return {0, 1};
		if (v >= 1)
			return {1, 1};
		return av_d2q(v, 100000);
	}

	void DropPlus(::AVFrame* raw, StormByte::Safe::String& warning, const char* why) noexcept {
		if (!raw || !av_frame_get_side_data(raw, AV_FRAME_DATA_DYNAMIC_HDR_PLUS))
			return;
		av_frame_remove_side_data(raw, AV_FRAME_DATA_DYNAMIC_HDR_PLUS);
		warning = std::string_view(why);
	}

	void ScaleEllipses(AVDynamicHDRPlus& plus, double sx, double sy) noexcept {
		for (unsigned i = 0; i < plus.num_windows && i < 3u; ++i) {
			auto& p = plus.params[i];
			p.center_of_ellipse_x = static_cast<uint16_t>(
				std::clamp(std::lround(p.center_of_ellipse_x * sx), 0L, 65535L));
			p.center_of_ellipse_y = static_cast<uint16_t>(
				std::clamp(std::lround(p.center_of_ellipse_y * sy), 0L, 65535L));
			p.semimajor_axis_internal_ellipse = static_cast<uint16_t>(
				std::clamp(std::lround(p.semimajor_axis_internal_ellipse * sx), 0L, 65535L));
			p.semimajor_axis_external_ellipse = static_cast<uint16_t>(
				std::clamp(std::lround(p.semimajor_axis_external_ellipse * sx), 0L, 65535L));
			p.semiminor_axis_external_ellipse = static_cast<uint16_t>(
				std::clamp(std::lround(p.semiminor_axis_external_ellipse * sy), 0L, 65535L));
		}
	}

	void MapWindowsToOrigin(AVDynamicHDRPlus& plus,
		int in_w, int in_h, int x0, int y0, int out_w, int out_h) noexcept {
		if (in_w <= 1 || in_h <= 1 || out_w <= 1 || out_h <= 1)
			return;
		const double iw = static_cast<double>(in_w - 1);
		const double ih = static_cast<double>(in_h - 1);
		const double ow = static_cast<double>(out_w - 1);
		const double oh = static_cast<double>(out_h - 1);
		for (unsigned i = 0; i < plus.num_windows && i < 3u; ++i) {
			auto& p = plus.params[i];
			const double x1 = Rat(p.window_upper_left_corner_x) * iw - x0;
			const double y1 = Rat(p.window_upper_left_corner_y) * ih - y0;
			const double x2 = Rat(p.window_lower_right_corner_x) * iw - x0;
			const double y2 = Rat(p.window_lower_right_corner_y) * ih - y0;
			p.window_upper_left_corner_x = MakeRat(std::clamp(x1 / ow, 0.0, 1.0));
			p.window_upper_left_corner_y = MakeRat(std::clamp(y1 / oh, 0.0, 1.0));
			p.window_lower_right_corner_x = MakeRat(std::clamp(x2 / ow, 0.0, 1.0));
			p.window_lower_right_corner_y = MakeRat(std::clamp(y2 / oh, 0.0, 1.0));
			const long cx = std::lround(static_cast<double>(p.center_of_ellipse_x) - x0);
			const long cy = std::lround(static_cast<double>(p.center_of_ellipse_y) - y0);
			p.center_of_ellipse_x = static_cast<uint16_t>(std::clamp(cx, 0L, static_cast<long>(out_w)));
			p.center_of_ellipse_y = static_cast<uint16_t>(std::clamp(cy, 0L, static_cast<long>(out_h)));
		}
	}
}

void Video::Put(const ::AVFrame* before, ::AVFrame* after) noexcept {
	m_warning.clear();
	m_diagnostic.clear();
	if (before && after) {
		for (const auto type : {AV_FRAME_DATA_DOVI_METADATA, AV_FRAME_DATA_DOVI_RPU_BUFFER,
			AV_FRAME_DATA_MASTERING_DISPLAY_METADATA, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL}) {
			const auto* source = av_frame_get_side_data(before, type);
			const auto* replacement = av_frame_get_side_data(after, type);
			const char* name = "unknown";
			switch (type) {
				case AV_FRAME_DATA_DOVI_METADATA: name = "dovi-metadata"; break;
				case AV_FRAME_DATA_DOVI_RPU_BUFFER: name = "dovi-rpu"; break;
				case AV_FRAME_DATA_MASTERING_DISPLAY_METADATA: name = "hdr10-mastering"; break;
				case AV_FRAME_DATA_CONTENT_LIGHT_LEVEL: name = "hdr10-content-light"; break;
				default: break;
			}
			if (!source) {
				m_diagnostic += std::format(" {}=source-absent", name);
				continue;
			}
			if (replacement) {
				m_diagnostic += std::format(" {}=already-present-kept:{}B", name, replacement->size);
				continue;
			}
			if (av_frame_side_data_clone(&after->side_data, &after->nb_side_data, source, 0) < 0) {
				m_diagnostic += std::format(" {}=copy-failed:{}B", name, source->size);
				m_warning = std::string_view("failed to preserve Dolby Vision/HDR10 frame side data");
			}
			else {
				m_diagnostic += std::format(" {}=copied:{}B", name, source->size);
			}
		}
		if ((av_frame_get_side_data(before, AV_FRAME_DATA_DOVI_METADATA)
			|| av_frame_get_side_data(before, AV_FRAME_DATA_DOVI_RPU_BUFFER)) && !Same(Read(before), Read(after))) {
			m_diagnostic += std::format(" dovi-geometry={}x{}->{}x{} crop={},{},{},{}->{},{},{},{}",
				before->width, before->height, after->width, after->height,
				before->crop_top, before->crop_bottom, before->crop_left, before->crop_right,
				after->crop_top, after->crop_bottom, after->crop_left, after->crop_right);
			m_warning = std::string_view("Dolby Vision metadata retained after geometry changes; RPU semantics may no longer match the image");
		}
		const auto* sourcePlus = av_frame_get_side_data(before, AV_FRAME_DATA_DYNAMIC_HDR_PLUS);
		const auto* replacementPlus = av_frame_get_side_data(after, AV_FRAME_DATA_DYNAMIC_HDR_PLUS);
		if (!sourcePlus)
			m_diagnostic += " hdr10plus=source-absent";
		else if (!replacementPlus)
			m_diagnostic += std::format(" hdr10plus=missing-on-replacement:{}B", sourcePlus->size);
		else
			m_diagnostic += std::format(" hdr10plus=already-present-kept:{}B", replacementPlus->size);
	}
	if (!after)
		return;
	if (!av_frame_get_side_data(after, AV_FRAME_DATA_DYNAMIC_HDR_PLUS))
		return;

	const Card in = Read(before);
	const Card out = Read(after);
	const Kind kind = Classify(in, out);

	if (kind == Kind::Identity) {
		m_diagnostic += " hdr10plus=kept-identity";
		return;
	}

	if (kind == Kind::Unknown) {
		m_diagnostic += " hdr10plus=dropped-unsupported-geometry";
		DropPlus(after, m_warning,
			"dropped HDR10+ (geometry is not a single Scale/Crop/Pad)");
		return;
	}

	if (kind == Kind::Pad) {
		m_diagnostic += " hdr10plus=dropped-pad-offset";
		DropPlus(after, m_warning,
			"dropped HDR10+ (pad offset is not on the card)");
		return;
	}

	AVFrameSideData* sd = av_frame_get_side_data(after, AV_FRAME_DATA_DYNAMIC_HDR_PLUS);
	if (!sd || !sd->data)
		return;
	auto* plus = reinterpret_cast<AVDynamicHDRPlus*>(sd->data);

	if (kind == Kind::Scale) {
		const double sx = static_cast<double>(out.width) / static_cast<double>(in.width);
		const double sy = static_cast<double>(out.height) / static_cast<double>(in.height);
		ScaleEllipses(*plus, sx, sy);
		m_diagnostic += std::format(" hdr10plus=remapped-scale:{:.6f}x{:.6f}", sx, sy);
		return;
	}

	int x0 = in.crop_left;
	int y0 = in.crop_top;
	if (in.crop_left == out.crop_left && in.crop_top == out.crop_top &&
		in.crop_right == out.crop_right && in.crop_bottom == out.crop_bottom) {
		x0 = 0;
		y0 = 0;
	}

	MapWindowsToOrigin(*plus, in.width, in.height, x0, y0, out.width, out.height);
	m_diagnostic += std::format(" hdr10plus=remapped-crop:{}x{}@{},{}",
		in.width, in.height, x0, y0);
}

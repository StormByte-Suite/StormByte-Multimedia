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

#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/video/format.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Format;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;

Format::Format(Safe::Shared<StormByte::Logger::Log> log, int pixFmt) noexcept
	: Filter::Process(std::move(log), Safe::String("format")),
	m_pixFmt(pixFmt) {}

Format::~Format() noexcept = default;

enum Type Format::Media() const noexcept {
	return Type::Video;
}

void Format::Clean() noexcept {}

void Format::Setup() noexcept {}

void Format::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Log(Level::Warning, "format: frame has no picture");
		return;
	}
	if (src.Hardware()) {
		Fail("format: hardware frame; decode to software first");
		return;
	}
	if (m_pixFmt == FFrame::FormatNone()) {
		Fail("format: destination pixel format is none");
		return;
	}
	if (src.Format() == m_pixFmt) {
		Log(Level::LowLevel, std::format(
			"format no-op {} pts={}", src.FormatName(), src.Pts()));
		return;
	}

	const bool packed = src.Layout() == FFrame::VideoLayout::Unknown
		|| m_pixFmt == FFrame::FormatRgba();
	const auto scaler = packed ? FFrame::Scaler::Sws : FFrame::Scaler::Zimg;

	FFrame out;
	out.Format(m_pixFmt);
	if (!src.ScaleTo(out, src.Width(), src.Height(),
			FFrame::Resample::Default, scaler) || !out.CopyProps(src)) {
		Fail("format: ScaleTo failed");
		return;
	}

	Log(Level::LowLevel, std::format(
		"format {} -> {} {}x{} pts={}",
		src.FormatName(), out.FormatName(), out.Width(), out.Height(), out.Pts()));
	Save(std::move(out));
}

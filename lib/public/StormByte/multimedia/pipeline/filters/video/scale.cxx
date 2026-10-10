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

#include <StormByte/multimedia/pipeline/filters/video/scale.hxx>

#include <format>

using namespace StormByte::Multimedia::Pipeline::Filter::Video;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using StormByte::Logger::Level;

Scale::Scale(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	const StormByte::Multimedia::Property::Resolution& resolution,
	StormByte::Safe::Optional<FFrame::Resample> filter,
	StormByte::Safe::Optional<FFrame::Scaler> scaler) noexcept
	: Filter::Process(std::move(log), StormByte::Safe::String("scale")),
	m_width(resolution.Width()), m_height(resolution.Height()),
	m_filter(filter), m_scaler(scaler) {}

Scale::Scale(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	std::uint32_t width, std::uint32_t height,
	StormByte::Safe::Optional<FFrame::Resample> filter,
	StormByte::Safe::Optional<FFrame::Scaler> scaler) noexcept
	: Filter::Process(std::move(log), StormByte::Safe::String("scale")),
	m_width(width), m_height(height),
	m_filter(filter), m_scaler(scaler) {}

Scale::~Scale() noexcept = default;

enum StormByte::Multimedia::Type Scale::Media() const noexcept {
	return StormByte::Multimedia::Type::Video;
}

void Scale::Clean() noexcept {}

void Scale::Setup() noexcept {
	Log(Level::Debug, std::format("target {}x{}", m_width, m_height));
}

void Scale::Process(const Pipeline::Frame&) noexcept {
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Fail("missing video buffer");
		return;
	}

	if (m_width == 0 && m_height == 0) {
		Fail("width and height are both 0");
		return;
	}

	std::uint32_t dstW = m_width;
	std::uint32_t dstH = m_height;
	if (dstW == 0)
		dstW = static_cast<std::uint32_t>(
			(static_cast<std::uint64_t>(src.Width()) * dstH + src.Height() / 2) / src.Height());
	if (dstH == 0)
		dstH = static_cast<std::uint32_t>(
			(static_cast<std::uint64_t>(src.Height()) * dstW + src.Width() / 2) / src.Width());
	if (dstW == 0 || dstH == 0) {
		Fail("computed destination is empty");
		return;
	}

	if (static_cast<int>(dstW) == src.Width() && static_cast<int>(dstH) == src.Height()) {
		Log(Level::LowLevel, std::format("passthrough {}x{}", src.Width(), src.Height()));
		return;
	}

	Log(Level::LowLevel, std::format("{}x{} -> {}x{}",
		src.Width(), src.Height(), dstW, dstH));

	const auto filter = m_filter.value_or(FFrame::Resample::Default);
	const auto scaler = m_scaler.value_or(FFrame::Scaler::Zimg);

	FFrame out;
	if (!src.ScaleTo(ScalingContext(), out, static_cast<int>(dstW), static_cast<int>(dstH), filter, scaler)) {
		Fail("scale failed");
		return;
	}

	Save(std::move(out));
}

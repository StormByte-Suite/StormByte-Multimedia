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

#include <StormByte/multimedia/ffmpeg/AVChannelLayout.hxx>

#include <string>
#include <vector>

extern "C" {
	#include <libavutil/channel_layout.h>
	#include <libavutil/mem.h>
}

namespace StormByte::Multimedia::FFmpeg {
	AVChannelLayout::AVChannelLayout() noexcept = default;

	AVChannelLayout::AVChannelLayout(int channels) noexcept {
		if (channels > 0) {
			Ensure();
			if (m_raw)
				av_channel_layout_default(m_raw, channels);
		}
	}

	AVChannelLayout::AVChannelLayout(const AVChannelLayout& other) noexcept {
		if (other.m_raw) {
			Ensure();
			if (m_raw && av_channel_layout_copy(m_raw, other.m_raw) < 0)
				Free();
		}
	}

	AVChannelLayout::AVChannelLayout(AVChannelLayout&& other) noexcept
	: m_raw(other.m_raw) {
		other.m_raw = nullptr;
	}

	AVChannelLayout::~AVChannelLayout() noexcept {
		Free();
	}

	AVChannelLayout& AVChannelLayout::operator=(const AVChannelLayout& other) noexcept {
		if (this == &other)
			return *this;
		if (!other.m_raw) {
			Free();
			return *this;
		}
		Ensure();
		if (m_raw && av_channel_layout_copy(m_raw, other.m_raw) < 0)
			Free();
		return *this;
	}

	AVChannelLayout& AVChannelLayout::operator=(AVChannelLayout&& other) noexcept {
		if (this == &other)
			return *this;
		Free();
		m_raw = other.m_raw;
		other.m_raw = nullptr;
		return *this;
	}

	AVChannelLayout AVChannelLayout::Default(int channels) noexcept {
		return AVChannelLayout(channels);
	}

	AVChannelLayout::operator bool() const noexcept {
		return m_raw != nullptr && m_raw->nb_channels > 0;
	}

	int AVChannelLayout::NbChannels() const noexcept {
		return m_raw ? m_raw->nb_channels : 0;
	}

	std::uint64_t AVChannelLayout::Mask() const noexcept {
		if (!m_raw || m_raw->order != AV_CHANNEL_ORDER_NATIVE)
			return 0;
		return m_raw->u.mask;
	}

	int AVChannelLayout::Order() const noexcept {
		return m_raw ? static_cast<int>(m_raw->order) : 0;
	}

	StormByte::Safe::String AVChannelLayout::Describe() const noexcept {
		if (!m_raw || m_raw->nb_channels <= 0)
			return {};
		char small[64];
		const int n = av_channel_layout_describe(m_raw, small, sizeof(small));
		if (n < 0)
			return {};
		if (n < static_cast<int>(sizeof(small)))
			return StormByte::Safe::String(small);
		std::vector<char> big(static_cast<std::size_t>(n) + 1u);
		if (av_channel_layout_describe(m_raw, big.data(), big.size()) < 0)
			return {};
		return StormByte::Safe::String(big.data());
	}

	bool AVChannelLayout::operator==(const AVChannelLayout& other) const noexcept {
		if (!m_raw || !other.m_raw)
			return m_raw == other.m_raw;
		return av_channel_layout_compare(m_raw, other.m_raw) == 0;
	}

	bool AVChannelLayout::operator!=(const AVChannelLayout& other) const noexcept {
		return !(*this == other);
	}

	void AVChannelLayout::Ensure() noexcept {
		if (m_raw)
			return;
		m_raw = static_cast<::AVChannelLayout*>(av_mallocz(sizeof(::AVChannelLayout)));
	}

	void AVChannelLayout::Free() noexcept {
		if (!m_raw)
			return;
		av_channel_layout_uninit(m_raw);
		av_free(m_raw);
		m_raw = nullptr;
	}

	const ::AVChannelLayout* AVChannelLayout::Get() const noexcept {
		return m_raw;
	}

	::AVChannelLayout* AVChannelLayout::Get() noexcept {
		return m_raw;
	}
}

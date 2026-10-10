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

#include <StormByte/multimedia/backend/ffmpeg/AVPool.hxx>

#include <algorithm>
#include <cerrno>
#include <limits>
#include <map>
#include <mutex>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/buffer.h>
	#include <libavutil/cpu.h>
	#include <libavutil/frame.h>
	#include <libavutil/imgutils.h>
	#include <libavutil/pixdesc.h>
}

using namespace StormByte::Multimedia::Backend::FFmpeg;

namespace {
	struct DecoderPool {
		std::mutex mutex;
		std::shared_ptr<AVPool> pool;
	};
}

AVPool::AVPool(Key key, const std::array<std::size_t, 4>& sizes) noexcept
: m_key(key) {
	for (std::size_t plane = 0; plane < m_pools.size(); ++plane) {
		if (sizes[plane] == 0)
			continue;
		m_pools[plane] = av_buffer_pool_init(sizes[plane], av_buffer_allocz);
		if (!m_pools[plane])
			m_valid = false;
	}
}

AVPool::~AVPool() noexcept {
	for (auto& pool : m_pools)
		av_buffer_pool_uninit(&pool);
}

std::shared_ptr<AVPool> AVPool::For(::AVCodecContext& context, const ::AVFrame& frame) noexcept {
	if (frame.width <= 0 || frame.height <= 0
		|| av_image_check_size(frame.width, frame.height, 0, &context) < 0)
		return {};
	const auto format = static_cast<AVPixelFormat>(frame.format);
	const auto* descriptor = av_pix_fmt_desc_get(format);
	if (!descriptor || (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL))
		return {};
	int width = frame.width;
	int height = frame.height;
	std::array<int, AV_NUM_DATA_POINTERS> stride_alignment{};
	avcodec_align_dimensions2(&context, &width, &height, stride_alignment.data());
	if (width <= 0 || height <= 0)
		return {};
	std::array<int, 4> strides{};
	for (;;) {
		if (av_image_fill_linesizes(strides.data(), format, width) < 0)
			return {};
		bool aligned = true;
		for (std::size_t plane = 0; plane < strides.size(); ++plane) {
			if (stride_alignment[plane] > 0 && strides[plane] % stride_alignment[plane] != 0)
				aligned = false;
		}
		if (aligned)
			break;
		const int increment = width & -width;
		if (width > std::numeric_limits<int>::max() - increment)
			return {};
		width += increment;
	}
	std::array<ptrdiff_t, 4> plane_strides{};
	std::copy(strides.begin(), strides.end(), plane_strides.begin());
	std::array<std::size_t, 4> sizes{};
	if (av_image_fill_plane_sizes(sizes.data(), format, height, plane_strides.data()) < 0)
		return {};
	const int alignment = std::max(64, static_cast<int>(av_cpu_max_align()));
	for (auto& size : sizes) {
		if (size == 0)
			continue;
		const auto padding = static_cast<std::size_t>(AV_INPUT_BUFFER_PADDING_SIZE + alignment);
		if (size > static_cast<std::size_t>(std::numeric_limits<int>::max()) - padding)
			return {};
		size += padding;
	}
	const Key key{frame.format, width, height, strides[0], strides[1], strides[2], strides[3], alignment};
	try {
		static std::mutex mutex;
		static std::map<Key, std::weak_ptr<AVPool>> registry;
		std::lock_guard lock(mutex);
		for (auto entry = registry.begin(); entry != registry.end();) {
			if (entry->second.expired())
				entry = registry.erase(entry);
			else
				++entry;
		}
		if (const auto found = registry.find(key); found != registry.end()) {
			if (auto pool = found->second.lock())
				return pool;
		}
		std::shared_ptr<AVPool> pool(new AVPool(key, sizes));
		if (!pool->m_valid)
			return {};
		registry[key] = pool;
		return pool;
	}
	catch (...) {
		return {};
	}
}

int AVPool::Fill(::AVFrame& frame) noexcept {
	if (!m_valid || frame.format != m_key[0] || frame.width <= 0 || frame.height <= 0
		|| frame.width > m_key[1] || frame.height > m_key[2] || frame.nb_extended_buf != 0)
		return AVERROR(EINVAL);
	for (unsigned plane = 0; plane < AV_NUM_DATA_POINTERS; ++plane) {
		if (frame.data[plane] || frame.buf[plane])
			return AVERROR(EINVAL);
	}
	frame.extended_data = frame.data;
	for (std::size_t plane = 0; plane < m_pools.size(); ++plane) {
		frame.linesize[plane] = m_key[plane + 3];
		if (!m_pools[plane])
			continue;
		frame.buf[plane] = av_buffer_pool_get(m_pools[plane]);
		if (!frame.buf[plane]) {
			av_frame_unref(&frame);
			return AVERROR(ENOMEM);
		}
		frame.data[plane] = frame.buf[plane]->data;
	}
	return 0;
}

int AVPool::Attach(::AVCodecContext& context) noexcept {
	if (context.codec_type != AVMEDIA_TYPE_VIDEO || !context.codec
		|| !(context.codec->capabilities & AV_CODEC_CAP_DR1) || context.opaque
		|| context.get_buffer2 != avcodec_default_get_buffer2)
		return 0;
	try {
		context.opaque = new DecoderPool;
		context.get_buffer2 = GetBuffer;
		return 0;
	}
	catch (...) {
		return AVERROR(ENOMEM);
	}
}

int AVPool::GetBuffer(::AVCodecContext* context, ::AVFrame* frame, int flags) noexcept {
	if (!context || !frame)
		return AVERROR(EINVAL);
	const auto* descriptor = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame->format));
	if (context->hw_frames_ctx || !descriptor || (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL))
		return avcodec_default_get_buffer2(context, frame, flags);
	auto* owner = static_cast<DecoderPool*>(context->opaque);
	if (!owner)
		return AVERROR(EINVAL);
	std::lock_guard lock(owner->mutex);
	auto pool = For(*context, *frame);
	if (!pool)
		return AVERROR(ENOMEM);
	owner->pool = std::move(pool);
	return owner->pool->Fill(*frame);
}

void AVPool::Release(::AVCodecContext*& context) noexcept {
	if (!context)
		return;
	DecoderPool* owner = context->get_buffer2 == GetBuffer
		? static_cast<DecoderPool*>(context->opaque) : nullptr;
	avcodec_free_context(&context);
	delete owner;
}

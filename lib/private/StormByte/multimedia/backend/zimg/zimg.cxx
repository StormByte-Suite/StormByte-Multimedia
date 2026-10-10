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

#include <StormByte/multimedia/backend/zimg/zimg.hxx>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace StormByte::Multimedia::Backend {
	namespace {
		constexpr unsigned kAlign = 64;

		void* Align64(std::vector<std::uint8_t>& buf, std::size_t bytes) noexcept {
			buf.resize(bytes + kAlign);
			const auto p = reinterpret_cast<std::uintptr_t>(buf.data());
			return reinterpret_cast<void*>((p + (kAlign - 1)) & ~std::uintptr_t(kAlign - 1));
		}

		int Depth(const FFmpeg::AVFrame& frame) noexcept {
			const int bpc = frame.BitsPerComponent();
			if (bpc <= 8)
				return 8;
			if (bpc <= 10)
				return 10;
			return 12;
		}

		void ApplyFilter(zimg_graph_builder_params& params,
			FFmpeg::AVFrame::Resample filter) noexcept {
			if (filter == FFmpeg::AVFrame::Resample::Default)
				return;
			zimg_resample_filter_e kernel = ZIMG_RESIZE_BICUBIC;
			switch (filter) {
				case FFmpeg::AVFrame::Resample::Point:    kernel = ZIMG_RESIZE_POINT; break;
				case FFmpeg::AVFrame::Resample::Bilinear: kernel = ZIMG_RESIZE_BILINEAR; break;
				case FFmpeg::AVFrame::Resample::Bicubic:  kernel = ZIMG_RESIZE_BICUBIC; break;
				case FFmpeg::AVFrame::Resample::Spline:   kernel = ZIMG_RESIZE_SPLINE36; break;
				case FFmpeg::AVFrame::Resample::Lanczos:  kernel = ZIMG_RESIZE_LANCZOS; break;
				case FFmpeg::AVFrame::Resample::Default:  break;
			}
			params.resample_filter = kernel;
			params.resample_filter_uv = kernel;
		}

		bool FillFormat(const FFmpeg::AVFrame& frame, zimg_image_format& fmt) noexcept {
			if (!frame || frame.Width() <= 0 || frame.Height() <= 0)
				return false;
			zimg_image_format_default(&fmt, ZIMG_API_VERSION);
			fmt.width = static_cast<unsigned>(frame.Width());
			fmt.height = static_cast<unsigned>(frame.Height());
			const int depth = Depth(frame);
			fmt.depth = static_cast<unsigned>(depth);
			fmt.pixel_type = (depth > 8) ? ZIMG_PIXEL_WORD : ZIMG_PIXEL_BYTE;
			switch (frame.Layout()) {
				case FFmpeg::AVFrame::VideoLayout::Gray:
					fmt.color_family = ZIMG_COLOR_GREY;
					fmt.subsample_w = 0;
					fmt.subsample_h = 0;
					break;
				case FFmpeg::AVFrame::VideoLayout::Yuv420:
					fmt.color_family = ZIMG_COLOR_YUV;
					fmt.subsample_w = 1;
					fmt.subsample_h = 1;
					break;
				case FFmpeg::AVFrame::VideoLayout::Yuv422:
					fmt.color_family = ZIMG_COLOR_YUV;
					fmt.subsample_w = 1;
					fmt.subsample_h = 0;
					break;
				case FFmpeg::AVFrame::VideoLayout::Yuv444:
					fmt.color_family = ZIMG_COLOR_YUV;
					fmt.subsample_w = 0;
					fmt.subsample_h = 0;
					break;
				default:
					return false;
			}
			fmt.field_parity = ZIMG_FIELD_PROGRESSIVE;
			fmt.alpha = ZIMG_ALPHA_NONE;
			return true;
		}

		void CopyColor(const FFmpeg::AVFrame& src, zimg_image_format& in,
			zimg_image_format& out) noexcept {
			in.pixel_range = (src.ColorRange() == 2)
				? ZIMG_RANGE_FULL : ZIMG_RANGE_LIMITED;
			const int matrix = src.ColorSpace();
			in.matrix_coefficients = (matrix > 0)
				? static_cast<zimg_matrix_coefficients_e>(matrix)
				: ZIMG_MATRIX_UNSPECIFIED;
			const int transfer = src.ColorTransfer();
			in.transfer_characteristics = (transfer > 0)
				? static_cast<zimg_transfer_characteristics_e>(transfer)
				: ZIMG_TRANSFER_UNSPECIFIED;
			const int primaries = src.ColorPrimaries();
			in.color_primaries = (primaries > 0)
				? static_cast<zimg_color_primaries_e>(primaries)
				: ZIMG_PRIMARIES_UNSPECIFIED;
			const int chroma = src.ChromaLocation();
			in.chroma_location = (chroma >= 0)
				? static_cast<zimg_chroma_location_e>(chroma)
				: ZIMG_CHROMA_LEFT;
			out.pixel_range = in.pixel_range;
			out.matrix_coefficients = in.matrix_coefficients;
			out.transfer_characteristics = in.transfer_characteristics;
			out.color_primaries = in.color_primaries;
			out.chroma_location = in.chroma_location;
		}

		void FillConstBuffer(const FFmpeg::AVFrame& frame, zimg_image_buffer_const& buf) noexcept {
			buf = {};
			buf.version = ZIMG_API_VERSION;
			const int planes = frame.PlaneCount();
			for (int i = 0; i < planes && i < 4; ++i) {
				buf.plane[i].data = frame.Data(i);
				buf.plane[i].stride = frame.Linesize(i);
				buf.plane[i].mask = ZIMG_BUFFER_MAX;
			}
		}

		void FillBuffer(FFmpeg::AVFrame& frame, zimg_image_buffer& buf) noexcept {
			buf = {};
			buf.version = ZIMG_API_VERSION;
			const int planes = frame.PlaneCount();
			for (int i = 0; i < planes && i < 4; ++i) {
				buf.plane[i].data = frame.Data(i);
				buf.plane[i].stride = frame.Linesize(i);
				buf.plane[i].mask = ZIMG_BUFFER_MAX;
			}
		}

		unsigned StripCount(int dstH) noexcept {
			unsigned hc = std::thread::hardware_concurrency();
			if (hc < 1)
				hc = 1;
			if (hc > 16)
				hc = 16;
			unsigned jobs = hc;
			if (dstH < 128)
				jobs = 1;
			while (jobs > 1 && static_cast<unsigned>(dstH) / jobs < 64)
				--jobs;
			return jobs < 1 ? 1 : jobs;
		}
	}

	Zimg::Zimg() noexcept = default;

	Zimg::Zimg(Zimg&& other) noexcept
	:	m_slots(std::move(other.m_slots)),
		m_current(nullptr) {
		other.StopWorkers();
		other.m_current = nullptr;
		if (!m_slots.empty())
			m_current = &m_slots.back();
	}

	Zimg::~Zimg() noexcept {
		StopWorkers();
		Free();
	}

	Zimg& Zimg::operator=(Zimg&& other) noexcept {
		if (this != &other) {
			StopWorkers();
			Free();
			other.StopWorkers();
			m_slots = std::move(other.m_slots);
			other.m_current = nullptr;
			m_current = m_slots.empty() ? nullptr : &m_slots.back();
		}
		return *this;
	}

	Zimg::operator bool() const noexcept {
		return m_current && !m_current->strips.empty() && m_current->strips.front().graph;
	}

	void Zimg::StopWorkers() noexcept {
		{
			std::lock_guard<std::mutex> lock(m_mu);
			m_stop = true;
		}
		m_cv.notify_all();
		for (auto& t : m_workers) {
			if (t.joinable())
				t.join();
		}
		m_workers.clear();
		m_stop = false;
		m_epoch = 0;
		m_finished = 0;
		m_jobSlot = nullptr;
	}

	void Zimg::EnsureWorkers(unsigned n) const noexcept {
		if (n <= 1)
			return;
		if (m_workers.size() == n - 1)
			return;
		const_cast<Zimg*>(this)->StopWorkers();
		try {
			m_workers.reserve(n - 1);
			for (unsigned i = 1; i < n; ++i)
				m_workers.emplace_back([this, i]() { WorkerMain(i); });
		} catch (...) {
			const_cast<Zimg*>(this)->StopWorkers();
		}
	}

	void Zimg::WorkerMain(unsigned id) const {
		unsigned seen = 0;
		for (;;) {
			std::unique_lock<std::mutex> lock(m_mu);
			m_cv.wait(lock, [&]() { return m_stop || m_epoch != seen; });
			if (m_stop)
				return;
			seen = m_epoch;
			const Slot* slot = m_jobSlot;
			const auto src = m_jobSrc;
			const auto dst = m_jobDst;
			lock.unlock();
			if (slot && id < slot->strips.size())
				m_jobOk[id] = ProcessStrip(slot->strips[id], src, dst, slot->subH) ? 1 : 0;
			{
				std::lock_guard<std::mutex> done(m_mu);
				++m_finished;
			}
			m_cv.notify_all();
		}
	}

	void Zimg::FreeSlot(Slot& slot) noexcept {
		for (auto& strip : slot.strips) {
			if (strip.graph) {
				zimg_filter_graph_free(strip.graph);
				strip.graph = nullptr;
			}
			strip.scratch.clear();
		}
		slot.strips.clear();
	}

	bool Zimg::BuildStrip(Strip& strip, const FFmpeg::AVFrame& src,
		const FFmpeg::AVFrame& dst, zimg_image_format in, zimg_image_format out,
		FFmpeg::AVFrame::Resample filter, unsigned top, unsigned height) noexcept {
		const double srcH = static_cast<double>(src.Height());
		const double dstH = static_cast<double>(dst.Height());
		in.active_region.left = 0;
		in.active_region.top = static_cast<double>(top) * srcH / dstH;
		in.active_region.width = static_cast<double>(src.Width());
		in.active_region.height = static_cast<double>(height) * srcH / dstH;
		out.width = static_cast<unsigned>(dst.Width());
		out.height = height;

		zimg_graph_builder_params params{};
		zimg_graph_builder_params_default(&params, ZIMG_API_VERSION);
		ApplyFilter(params, filter);
		params.cpu_type = ZIMG_CPU_AUTO_64B;
		params.allow_approximate_gamma = 1;

		strip.graph = zimg_filter_graph_build(&in, &out, &params);
		if (!strip.graph)
			return false;
		std::size_t tmp = 0;
		if (zimg_filter_graph_get_tmp_size(strip.graph, &tmp) != 0) {
			zimg_filter_graph_free(strip.graph);
			strip.graph = nullptr;
			return false;
		}
		(void)Align64(strip.scratch, tmp ? tmp : 1);
		strip.outTop = top;
		strip.outHeight = height;
		return true;
	}

	bool Zimg::BuildSlot(Slot& slot, const FFmpeg::AVFrame& src,
		const FFmpeg::AVFrame& dst, FFmpeg::AVFrame::Resample filter) noexcept {
		zimg_image_format in{};
		zimg_image_format out{};
		if (!FillFormat(src, in) || !FillFormat(dst, out))
			return false;
		CopyColor(src, in, out);

		FreeSlot(slot);
		const unsigned jobs = StripCount(dst.Height());
		const unsigned full = static_cast<unsigned>(dst.Height());
		unsigned top = 0;
		slot.strips.resize(jobs);
		slot.subH = out.subsample_h;
		for (unsigned i = 0; i < jobs; ++i) {
			unsigned end = full * (i + 1) / jobs;
			end &= ~1u;
			if (i + 1 == jobs)
				end = full;
			if (end <= top) {
				FreeSlot(slot);
				return false;
			}
			if (!BuildStrip(slot.strips[i], src, dst, in, out, filter, top, end - top)) {
				FreeSlot(slot);
				return false;
			}
			top = end;
		}

		slot.srcW = src.Width();
		slot.srcH = src.Height();
		slot.srcFmt = src.Format();
		slot.dstW = dst.Width();
		slot.dstH = dst.Height();
		slot.dstFmt = dst.Format();
		slot.filter = static_cast<int>(filter);
		return true;
	}

	bool Zimg::ProcessStrip(const Strip& strip,
		const zimg_image_buffer_const& srcBuf, const zimg_image_buffer& dstBuf,
		unsigned subH) noexcept {
		if (!strip.graph)
			return false;
		zimg_image_buffer dst = dstBuf;
		for (int i = 0; i < 4; ++i) {
			if (!dst.plane[i].data)
				continue;
			const unsigned shift = (i == 0) ? 0u : subH;
			auto* base = static_cast<std::uint8_t*>(dst.plane[i].data);
			dst.plane[i].data = base + dst.plane[i].stride * static_cast<int>(strip.outTop >> shift);
		}
		auto* tmp = Align64(const_cast<std::vector<std::uint8_t>&>(strip.scratch),
			strip.scratch.empty() ? 1 : strip.scratch.size() > kAlign
				? strip.scratch.size() - kAlign : 1);
		return zimg_filter_graph_process(strip.graph, &srcBuf, &dst, tmp,
			nullptr, nullptr, nullptr, nullptr) == 0;
	}

	void Zimg::Free() noexcept {
		for (auto& slot : m_slots)
			FreeSlot(slot);
		m_slots.clear();
		m_current = nullptr;
	}

	Zimg Zimg::Open(const FFmpeg::AVFrame& src, const FFmpeg::AVFrame& dst,
		FFmpeg::AVFrame::Resample filter) noexcept {
		Zimg out;
		(void)out.Ensure(src, dst, filter);
		return out;
	}

	bool Zimg::Ensure(const FFmpeg::AVFrame& src, const FFmpeg::AVFrame& dst,
		FFmpeg::AVFrame::Resample filter) noexcept {
		const int srcW = src.Width();
		const int srcH = src.Height();
		const int srcFmt = src.Format();
		const int dstW = dst.Width();
		const int dstH = dst.Height();
		const int dstFmt = dst.Format();
		const int key = static_cast<int>(filter);

		for (auto& slot : m_slots) {
			if (!slot.strips.empty()
				&& slot.srcW == srcW && slot.srcH == srcH && slot.srcFmt == srcFmt
				&& slot.dstW == dstW && slot.dstH == dstH && slot.dstFmt == dstFmt
				&& slot.filter == key) {
				m_current = &slot;
				EnsureWorkers(static_cast<unsigned>(slot.strips.size()));
				return true;
			}
		}

		m_slots.emplace_back();
		if (!BuildSlot(m_slots.back(), src, dst, filter)) {
			m_slots.pop_back();
			m_current = nullptr;
			return false;
		}
		m_current = &m_slots.back();
		EnsureWorkers(static_cast<unsigned>(m_current->strips.size()));
		return true;
	}

	bool Zimg::Scale(const FFmpeg::AVFrame& src, FFmpeg::AVFrame& dst) const noexcept {
		if (!m_current || m_current->strips.empty() || !dst.Data(0))
			return false;

		zimg_image_buffer_const srcBuf{};
		zimg_image_buffer dstBuf{};
		FillConstBuffer(src, srcBuf);
		FillBuffer(dst, dstBuf);

		auto& strips = m_current->strips;
		if (strips.size() == 1)
			return ProcessStrip(strips[0], srcBuf, dstBuf, m_current->subH);

		{
			std::unique_lock<std::mutex> lock(m_mu);
			m_jobSlot = m_current;
			m_jobSrc = srcBuf;
			m_jobDst = dstBuf;
			m_jobOk.assign(strips.size(), 0);
			m_finished = 0;
			++m_epoch;
		}
		m_cv.notify_all();

		m_jobOk[0] = ProcessStrip(strips[0], srcBuf, dstBuf, m_current->subH) ? 1 : 0;

		{
			std::unique_lock<std::mutex> lock(m_mu);
			m_cv.wait(lock, [&]() {
				return m_finished >= strips.size() - 1;
			});
		}
		return std::all_of(m_jobOk.begin(), m_jobOk.end(), [](int v) { return v != 0; });
	}
}

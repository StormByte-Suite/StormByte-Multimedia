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

#include <StormByte/multimedia/ffmpeg/AVBSFPipeline.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/backend/ffmpeg/AVPool.hxx>

using namespace StormByte::Multimedia::FFmpeg;

AVBSFPipeline::AVBSFPipeline() noexcept = default;

AVBSFPipeline::~AVBSFPipeline() noexcept {
	StormByte::Multimedia::Backend::FFmpeg::AVPool::ShutdownTrace("AVBSFPipeline filters clear begin", this);
	m_filters.clear();
	StormByte::Multimedia::Backend::FFmpeg::AVPool::ShutdownTrace("AVBSFPipeline filters clear end", this);
}

AVBSFPipeline::AVBSFPipeline(AVBSFPipeline&& other) noexcept:
m_filters(std::move(other.m_filters)) {
	other.m_filters.clear();
}

AVBSFPipeline& AVBSFPipeline::operator=(AVBSFPipeline&& other) noexcept {
	if (this != &other) {
		m_filters = std::move(other.m_filters);
		other.m_filters.clear();
	}

	return *this;
}

void AVBSFPipeline::Add(AVBSF&& bsf) noexcept {
	auto filter = StormByte::Safe::Shared<AVBSF>::MakePointer<AVBSF>(std::move(bsf));
	m_filters.push_back(std::move(filter));
}

OperationResult AVBSFPipeline::Process(AVPacket& pkt) noexcept {
	if (m_filters.empty())
		return OperationResult::Success;

	// Copy of the packet to process through the BSF chain
	AVPacket current = pkt.Ref();
	AVPacket next;

	for (auto& bsf : m_filters) {
		// Send the current packet to the BSF
		auto send_res = bsf->SendPacket(current);
		if (send_res != OperationResult::Success &&
			send_res != OperationResult::TryAgain)
			return send_res;

		// Receive the filtered packet into `next`
		auto recv_res = bsf->ReceivePacket(next);
		if (recv_res != OperationResult::Success)
			return recv_res;

		// The output of this filter becomes the input of the next one
		current = next.Ref();
	}

	// Finally, the final result is returned in `pkt`
	pkt = std::move(next);

	return OperationResult::Success;
}

void AVBSFPipeline::Flush() noexcept {
	for (auto& bsf : m_filters)
		bsf->Flush();
}

void AVBSFPipeline::SetEof() noexcept {
	for (auto& bsf : m_filters)
		bsf->SetEof();
}

void AVBSFPipeline::Clear() noexcept {
	m_filters.clear();
}

bool AVBSFPipeline::Empty() const noexcept {
	return m_filters.empty();
}

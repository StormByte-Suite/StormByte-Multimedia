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

#include <StormByte/multimedia/backend/pipeline/ceiling.hxx>
#include <StormByte/multimedia/pipeline/config/audio.hxx>
#include <StormByte/multimedia/pipeline/config/subtitle.hxx>
#include <StormByte/multimedia/pipeline/config/video.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Producer;
using StormByte::Multimedia::Pipeline::Track;
namespace Config = StormByte::Multimedia::Pipeline::Config;

namespace {
	bool Encodes(const Track& track) noexcept {
		const auto* cfg = track.Config();
		if (!cfg)
			return false;
		if (const auto* video = dynamic_cast<const Config::Video*>(cfg))
			return video->Codec() != nullptr;
		if (const auto* audio = dynamic_cast<const Config::Audio*>(cfg))
			return audio->Codec() != nullptr;
		if (const auto* sub = dynamic_cast<const Config::Subtitle*>(cfg))
			return sub->Codec() != nullptr;
		return false;
	}

	std::size_t AvailableRam() noexcept {
		std::ifstream in("/proc/meminfo");
		if (!in)
			return 8ull * 1024ull * 1024ull * 1024ull;
		std::string key;
		unsigned long long kb = 0;
		while (in >> key >> kb) {
			if (key == "MemAvailable:")
				return kb * 1024ull;
			std::string rest;
			std::getline(in, rest);
		}
		return 8ull * 1024ull * 1024ull * 1024ull;
	}

	// Decoded video is the expensive unit (~16 MiB per 4K P010 frame). Packets
	// and remux audio are cheap. The encoder lookahead (x265 default 20 + frame
	// threads + one in flight) must stay filled or the encode thread stalls and
	// the rest of the pipe goes idle. Cap at 32 so a second raw queue cannot
	// double RSS. Floor at 2 so a bind with a live plan never opens an unbounded
	// hopper. RAM/8/frame-size is a machine-wide clamp, not a per-track budget.
	std::size_t VideoFrames() noexcept {
		constexpr std::size_t look = 20 + 6 + 1;
		constexpr std::size_t bytes = 16ull * 1024ull * 1024ull;
		const std::size_t ram = AvailableRam() / 8ull / bytes;
		std::size_t n = look;
		if (ram < n)
			n = ram;
		if (n < 2)
			n = 2;
		if (n > 32)
			n = 32;
		return n;
	}

	// Compressed packets and remux audio. Measured on mixed encode+remux jobs:
	// a cap of 32 (hardware_concurrency on a 32-thread box) lets remux tracks
	// run tens of seconds of PTS ahead of the video encode and dump 16–32
	// packets into the muxer between two encoder outputs. A cap of 4 cuts the
	// dump but multiplies Remuxer/Muxer wait/wake with no RSS or CPU gain.
	// Eight is one short audio burst (~250 ms at 32 ms/packet) and keeps the
	// muxer from sleeping on an empty video slot while audio floods.
	std::size_t CheapPackets() noexcept {
		return 8;
	}

	std::size_t RemuxPackets() noexcept {
		return CheapPackets();
	}
}

namespace StormByte::Multimedia::Backend::Pipeline {
	unsigned Ceiling::MaxThreads() noexcept {
		static const unsigned cached = [] {
			unsigned n = std::thread::hardware_concurrency();
			return n < 1 ? 1u : n;
		}();
		return cached;
	}

	Ceiling::Ceiling(StormByte::Safe::Shared<const Multimedia::Pipeline::Plan> plan,
		Multimedia::Pipeline::Producer producer,
		const Multimedia::Pipeline::Track& track) noexcept
	: m_frames(0), m_packets(0), m_park(0) {
		if (!plan || plan->Tracks().empty())
			std::abort();
		if (track.Type() == Type::Attachment || track.Type() == Type::Unknown)
			return;
		const bool encode = Encodes(track);
		const bool video = track.Type() == Type::Video;
		const bool audio = track.Type() == Type::Audio;
		const std::size_t raw = VideoFrames();
		const std::size_t cheap = CheapPackets();
		const std::size_t remux = RemuxPackets();
		switch (producer) {
			case Producer::Demuxer:
				// Park + out hopper share the remux budget so the file reader
				// cannot prefetch a full GOP of every track while video encode
				// is one frame behind.
				m_packets = remux;
				m_park = remux;
				break;
			case Producer::Decoder:
				if (!encode)
					break;
				if (video) {
					m_frames = raw;
					m_packets = raw;
				}
				else {
					m_packets = cheap;
					if (audio)
						m_frames = cheap;
				}
				break;
			case Producer::Encoder:
				if (!encode)
					break;
				if (video) {
					// Raw in = lookahead. Encoded out is cheap; keep it on the
					// remux budget so the muxer does not buffer a second of
					// compressed video while waiting on other tracks.
					m_frames = raw;
					m_packets = remux;
				}
				else {
					m_packets = remux;
					if (audio)
						m_frames = cheap;
				}
				break;
			case Producer::Remuxer:
				if (!encode)
					m_packets = remux;
				break;
			case Producer::Muxer:
				m_packets = remux;
				break;
			case Producer::Filter:
				if (!encode)
					break;
				if (video)
					m_frames = raw;
				else
					m_packets = cheap;
				break;
			case Producer::Filters:
			case Producer::Route:
				break;
		}
	}

	std::size_t Ceiling::Frames() const noexcept {
		return m_frames;
	}

	std::size_t Ceiling::Packets() const noexcept {
		return m_packets;
	}

	std::size_t Ceiling::Park() const noexcept {
		return m_park;
	}
}

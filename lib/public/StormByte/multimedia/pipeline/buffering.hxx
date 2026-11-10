/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Multimedia.
 *
 * StormByte-Multimedia original source is dual-licensed:
 *
 * 1. GNU Lesser General Public License v3.0 (or later)
 *    You may redistribute and/or modify this file under the terms of the
 *    GNU Lesser General Public License, version 3, or (at your option) any
 *    later version.
 *
 * 2. Commercial license
 *    Alternatively, this file may be used under the terms of a commercial
 *    license agreement with the copyright holder
 *    (David C. Manuelda <StormByte@gmail.com>).
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#pragma once

#include <StormByte/buffer/byte_budget.hxx>
#include <StormByte/byte_size.hxx>
#include <StormByte/size.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>
#include <StormByte/type_traits/safe.hxx>

#include <cstdint>

namespace StormByte::Multimedia::Pipeline {
	/**
	 * @class QueueLimit
	 * @brief Automatic, explicitly bounded, or unlimited queue element cap.
	 *
	 * A bounded value of zero is invalid. Zero is never overloaded to mean
	 * automatic or unlimited.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC QueueLimit {
		public:
			enum class Mode: std::uint8_t {
				Automatic,
				Unlimited,
				Bounded
			};

			constexpr QueueLimit() noexcept = default;

			static constexpr QueueLimit Automatic() noexcept {
				return QueueLimit{Mode::Automatic, 0};
			}

			static constexpr QueueLimit Unlimited() noexcept {
				return QueueLimit{Mode::Unlimited, 0};
			}

			static constexpr QueueLimit Bounded(StormByte::Size value) noexcept {
				return QueueLimit{Mode::Bounded, value};
			}

			constexpr Mode GetMode() const noexcept {
				return m_mode;
			}

			constexpr StormByte::Size Value() const noexcept {
				return m_value;
			}

			constexpr bool Valid(StormByte::Size minimum) const noexcept {
				return m_mode != Mode::Bounded || m_value >= minimum;
			}

			constexpr StormByte::Size Resolve(StormByte::Size automatic) const noexcept {
				switch (m_mode) {
					case Mode::Automatic: return automatic;
					case Mode::Unlimited: return 0;
					case Mode::Bounded: return m_value;
				}
				return automatic;
			}

		private:
			constexpr QueueLimit(Mode mode, StormByte::Size value) noexcept
			: m_mode(mode), m_value(value) {}

			Mode m_mode = Mode::Automatic;
			StormByte::Size m_value = 0;
	};

	/**
	 * @class BufferingPolicy
	 * @brief Queue element-cap overrides shared by Plans and Transcoders.
	 *
	 * Automatic preserves the selected @ref ResourceUsage profile. Unlimited
	 * disables that element cap. QueueBytes is an optional local byte limit;
	 * Budget supplies aggregate reservations shared between queues and jobs.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC BufferingPolicy {
		public:
			constexpr QueueLimit VideoFrames() const noexcept { return m_video_frames; }
			constexpr QueueLimit AudioFrames() const noexcept { return m_audio_frames; }
			constexpr QueueLimit Packets() const noexcept { return m_packets; }
			constexpr QueueLimit DemuxPending() const noexcept { return m_demux_pending; }

			const StormByte::Safe::Optional<StormByte::ByteSize>& QueueBytes() const noexcept {
				return m_queue_bytes;
			}

			const StormByte::Buffer::ByteBudget& Budget() const noexcept {
				return m_budget;
			}

			constexpr void VideoFrames(QueueLimit limit) noexcept { m_video_frames = limit; }
			constexpr void AudioFrames(QueueLimit limit) noexcept { m_audio_frames = limit; }
			constexpr void Packets(QueueLimit limit) noexcept { m_packets = limit; }
			constexpr void DemuxPending(QueueLimit limit) noexcept { m_demux_pending = limit; }

			void QueueBytes(StormByte::Safe::Optional<StormByte::ByteSize> limit) noexcept {
				m_queue_bytes = limit;
			}

			void Budget(const StormByte::Buffer::ByteBudget& budget) noexcept {
				m_budget = budget;
			}

			constexpr bool Valid() const noexcept {
				return m_video_frames.Valid(2)
					&& m_audio_frames.Valid(1)
					&& m_packets.Valid(1)
					&& m_demux_pending.Valid(1);
			}

		private:
			QueueLimit m_video_frames;
			QueueLimit m_audio_frames;
			QueueLimit m_packets;
			QueueLimit m_demux_pending;
			StormByte::Safe::Optional<StormByte::ByteSize> m_queue_bytes;
			StormByte::Buffer::ByteBudget m_budget;
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::QueueLimit);
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::BufferingPolicy);

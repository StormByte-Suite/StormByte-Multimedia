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

#pragma once

#include <StormByte/bitmask.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/multimedia/features.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/atomic.hxx>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	class Decoder;
	class Frame;
}

namespace StormByte::Multimedia::Backend::Pipeline::Detail::Worker {
	class Decode;
}

/**
 * @namespace StormByte::Multimedia::Pipeline
 * @brief Demux / decode / filter / encode / mux types.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Pipeline {
	class Decoder;
	class Demuxer;
	class Filters;
	class Frame;
	class Packet;
	class Route;

	/**
	 * @enum DecoderFlag
	 * @brief Decoder behaviour bits. Empty mask is passthrough.
	 */
	enum class DecoderFlag: std::uint8_t {
		HeuristicsHDR10 = 1u << 0
	};

	/**
	 * @class DecoderFlags
	 * @brief Bitmask of DecoderFlag.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC DecoderFlags: public StormByte::Bitmask<DecoderFlags, DecoderFlag> {
		public:
			using Bitmask::Bitmask;
	};

	inline const DecoderFlags Heuristics{DecoderFlag::HeuristicsHDR10};	///< All current heuristic bits.

	/**
	 * @brief Binds one origin track of @p demuxer to @p decoder.
	 * @param demuxer Origin demuxer.
	 * @param decoder Destination decoder.
	 * @return @p decoder.
	 */
	STORMBYTE_MULTIMEDIA_PUBLIC Decoder& operator>>(Demuxer& demuxer, Decoder& decoder) noexcept;

	/**
	 * @class Decoder
	 * @brief Decodes packets of one origin track into frames.
	 *
	 * Each produced frame carries the serial number and decoding
	 * timestamp of its source packet. Parts distinguish frames from
	 * the same packet; serial numbers are not decoded-frame counts.
	 *
	 * @ref Filters can supply decoded pictures for analytics on
	 * compressed-packet connections. Source pictures identify
	 * @ref Producer::Decoder; destination pictures identify the
	 * supplying @ref Producer::Encoder or @ref Producer::Remuxer.
	 *
	 * Two-pass processing completes the measurement pass before
	 * decoding the source again for the output pass.
	 * Logging identifies the selected decoder and origin track;
	 * the supplied logger controls throttling.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Decoder final: public Step {
		friend class Backend::Pipeline::Decoder;
		friend class Backend::Pipeline::Detail::Worker::Decode;
		friend class Demuxer;
		friend class Filters;
		friend class Route;
		friend Decoder& operator>>(Demuxer& demuxer, Decoder& decoder) noexcept;

		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Decoder for one origin track.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param track Origin stream index.
			 * @param flags Heuristics / future bits. Empty = passthrough.
			 *
			 * Origin mode. Frames carry @ref Producer::Decoder.
			 * Setup waits for demuxer >> decoder.
			 */
			explicit Decoder(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				int track, DecoderFlags flags = DecoderFlags{}) noexcept;

			/**
			 * @brief Copy constructor.
			 * @param other Source decoder.
			 */
			Decoder(const Decoder& other) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Decoder to take.
			 */
			Decoder(Decoder&& other) noexcept = delete;

			/**
			 * @brief Destructor. @ref Step::Join Halt s before backends die.
			 */
			~Decoder() noexcept override;

			/**
			 * @brief Copy assignment.
			 * @param other Source decoder.
			 * @return *this.
			 */
			Decoder& operator=(const Decoder& other) = delete;

			/**
			 * @brief Move assignment.
			 * @param other Decoder to take.
			 * @return *this.
			 */
			Decoder& operator=(Decoder&& other) noexcept = delete;

			/**
			 * @brief true if open and not failed.
			 * @return Open and not Failed.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Track
			 * @{
			 */

			/**
			 * @brief Bound origin track.
			 * @return Track.
			 */
			inline int Index() const noexcept {
				return m_index;
			}

			/**
			 * @brief Maximum number of queued input packets.
			 * @return Packet limit, or `0` if no input queue exists.
			 */
			std::size_t InputCeiling() const noexcept override;

			/**
			 * @brief Flags.
			 * @return Mask.
			 */
			inline const DecoderFlags& Flags() const noexcept {
				return m_flags;
			}

			/**
			 * @brief Replaces flags (before demuxer >> decoder).
			 * @param flags New mask.
			 */
			inline void Flags(DecoderFlags flags) noexcept {
				m_flags = flags;
			}

			/**
			 * @brief Stream language tag copied from the origin File.
			 * @return Language, or empty.
			 */
			inline const StormByte::Safe::Optional<StormByte::Safe::String>& Language() const noexcept {
				return m_language;
			}

			/**
			 * @brief Stream title tag copied from the origin File.
			 * @return Title, or empty.
			 */
			inline const StormByte::Safe::Optional<StormByte::Safe::String>& Title() const noexcept {
				return m_title;
			}

			/**
			 * @}
			 */

			/**
			 * @name Implementation selection
			 * @{
			 */

			/**
			 * @brief Pinned FFmpeg decoder name, if any.
			 * @return Name, or empty before pin / auto-select.
			 */
			inline const StormByte::Safe::Optional<StormByte::Safe::String>& Implementation() const noexcept {
				return m_implementation;
			}

			/**
			 * @brief Pins an FFmpeg decoder name (before demuxer >> decoder).
			 * @param name Table name. Empty clears the pin.
			 */
			void Implementation(StormByte::Safe::String name) noexcept;

			/**
			 * @brief Extra required Feature bits.
			 * @return Mask.
			 */
			inline const Features& Require() const noexcept {
				return m_require;
			}

			/**
			 * @brief Replaces extra required Feature bits.
			 * @param features Bits the chosen table row must have.
			 */
			inline void Require(Features features) noexcept {
				m_require = features;
			}

			/**
			 * @brief Features of the opened implementation.
			 * @return Mask.
			 */
			inline const Features& Capabilities() const noexcept {
				return m_capabilities;
			}

			/**
			 * @}
			 */

		private:
			/**
			 * @class EncodeLook
			 * @brief Tag. Dest look after an Encoder. Only Route constructs it.
			 */
			struct EncodeLook {};

			/**
			 * @class RemuxLook
			 * @brief Tag. Dest look after a Remuxer. Only Route constructs it.
			 */
			struct RemuxLook {};

			/**
			 * @class SourceLook
			 * @brief Tag. Packet-origin look. Only Route constructs it.
			 */
			struct SourceLook {};

		public:
			/**
			 * @brief Post-encode recon for Analytics.
			 * @param log Shared logger.
			 * @param track Origin stream index (same as Encoder).
			 * @param tag Encode-look tag.
			 *
			 * Not callable from user code. Open does not wait for a
			 * Demuxer. The first Packet that carries codec parameters
			 * opens the backend. Produced frames are stamped
			 * @ref Producer::Encoder. Label is Decoder(look encode).
			 */
			Decoder(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				int track, EncodeLook tag) noexcept;

			/**
			 * @brief Post-remux recon for Analytics.
			 * @param log Shared logger.
			 * @param track Origin stream index (same as Remuxer).
			 * @param tag Remux-look tag.
			 *
			 * Not callable from user code. Same open path as
			 * @ref EncodeLook. Produced frames are stamped
			 * @ref Producer::Remuxer. Label is Decoder(look remux).
			 */
			Decoder(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				int track, RemuxLook tag) noexcept;

			/**
			 * @brief Packet-origin recon for Analytics (Demuxer remux stretch).
			 * @param log Shared logger.
			 * @param track Origin stream index.
			 * @param tag Source-look tag.
			 *
			 * Not callable from user code. Same open path as
			 * @ref EncodeLook (codecpar on first Packet). Produced
			 * frames keep @ref Producer::Decoder.
			 */
			Decoder(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				int track, SourceLook tag) noexcept;

		private:
			using Step::Log;

			/**
			 * @brief Token after `STMM ` for this decoder.
			 * @return `Decoder(look encode|remux|src t=<index>)` in look
			 *         mode, otherwise `Decoder(<implementation>)` or
			 *         `Decoder(t=<index>)`.
			 */
			StormByte::Safe::String Label() const noexcept override;

			/**
			 * @brief Pins the opened backend.
			 * @param backend Opened decode backend.
			 */
			void Bind(StormByte::Safe::Unique<Backend::Pipeline::Decoder> backend) noexcept;

			/**
			 * @brief Opens the decode backend from the bound demuxer.
			 * @return Backend, or empty after Fail.
			 */
			StormByte::Safe::Unique<Backend::Pipeline::Decoder> OpenOrigin() noexcept;

			/**
			 * @brief Copies origin File tags onto this decoder.
			 * @param language Stream language, or empty.
			 * @param title Stream title, or empty.
			 */
			void Stamp(StormByte::Safe::Optional<StormByte::Safe::String> language,
				StormByte::Safe::Optional<StormByte::Safe::String> title) noexcept;

			/**
			 * @brief Records the demuxer of demuxer >> decoder.
			 * @param demuxer Origin demuxer.
			 */
			void AttachOrigin(Demuxer& demuxer) noexcept;

			/**
			 * @brief Measure origin will not push more packets.
			 *
			 * Friend: Filters. Sets @ref m_measureClosed and wakes
			 * the decode worker. Does not drain. Drain is the
			 * decode worker (@ref AfterWait or after the last packet).
			 */
			void MeasureSourceClosed() noexcept;

			/**
			 * @brief Flush leftover codec frames, ResetAfterMeasure,
			 *        tell Filters this track is drained.
			 *
			 * Once, via @ref m_measureDrained. Decode worker only.
			 * Does not clear @ref m_measureClosed.
			 */
			void DrainMeasure() noexcept;

			/**
			 * @brief Drain the codec and open it again. Same hopper.
			 * @return false if the backend could not reset (Fail already ran).
			 *
			 * Friend-only. Origin-mode Decoder only. Look decoders
			 * return true and do nothing. Does not Stop the Step.
			 */
			bool ResetAfterMeasure() noexcept;

			/**
			 * @brief Binds a decoded handle and copies stream tags onto @p frame.
			 * @param frame Public unit.
			 * @param backend Holder of the FFmpeg frame. May be empty.
			 */
			void Attach(Frame& frame, StormByte::Safe::Unique<Backend::Pipeline::Frame> backend) noexcept;

			/**
			 * @brief Closes the duration of a subtitle cue on @p frame.
			 * @param frame Public unit produced by this decoder.
			 * @param duration Cue length on the stream clock.
			 */
			void CloseCue(Frame& frame, Property::Duration duration) noexcept;

			/**
			 * @brief Copies pipe lineage and packet Dts onto @p frame.
			 * @param frame Public unit produced by this decoder.
			 */
			void StampLineage(Frame& frame) noexcept;

			/**
			 * @brief Opens the look backend from a Packet's codecpar.
			 * @param packet First look Packet that carries parameters.
			 * @return true if the backend is open.
			 */
			bool OpenLook(const Packet& packet) noexcept;

			/**
			 * @brief Stamps dest-look Producer on @p frame.
			 * @param frame Frame just received from the backend.
			 *
			 * Encode-look → @ref Producer::Encoder.
			 * Remux-look → @ref Producer::Remuxer.
			 * Source-look stays @ref Producer::Decoder.
			 * Dest-look also reports pts to Filters for the
			 * analytics axis.
			 */
			void StampLook(Frame& frame) noexcept;

			/**
			 * @brief Attaches the Filters facade that owns this dest look.
			 * @param filters Tube facade.
			 *
			 * Friend: Filters::Close. Origin-mode decoders leave
			 * this null. Leaves never call this.
			 */
			void BindAnalytics(Filters& filters) noexcept;

			/**
			 * @brief Wake Wait when measure origin is closed and In is empty.
			 */
			bool WakeNow() const noexcept override;

			/**
			 * @brief Drain measure on the decode worker after Wait.
			 */
			void AfterWait() noexcept override;

			int m_index;											///< Origin stream index
			DecoderFlags m_flags;									///< Heuristic bits
			StormByte::Safe::Optional<StormByte::Safe::String> m_language;			///< Origin language tag
			StormByte::Safe::Optional<StormByte::Safe::String> m_title;				///< Origin title tag
			StormByte::Safe::Optional<StormByte::Safe::String> m_implementation;	///< Pinned decoder name
			Features m_require;										///< Extra required bits
			Features m_capabilities;								///< Opened row bits
			Demuxer* m_origin = nullptr;							///< Origin demuxer, origin mode
			Filters* m_analytics = nullptr;							///< Dest-look clock; null on origin decode
			StormByte::Safe::Unique<Backend::Pipeline::Decoder> m_backend;	///< Decode backend
			StormByte::Safe::Optional<std::uint64_t> m_serial;	///< Last packet Serial
			std::uint64_t m_part;									///< Part within Serial
			StormByte::Safe::Optional<Property::Duration> m_inDts;		///< Dts of last packet
			bool m_look = false;									///< Opens from Packet codecpar
			StormByte::Safe::Optional<Producer> m_lookStamp;	///< Dest-look Producer, or empty
			StormByte::Safe::Atomic<bool> m_measureClosed{false};	///< Origin will not send more measure packets
			StormByte::Safe::Atomic<bool> m_measureDrained{false};	///< OnMeasureDrained already ran
			Join m_join{*this};										///< Halt before other members die
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::DecoderFlags);
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Decoder);

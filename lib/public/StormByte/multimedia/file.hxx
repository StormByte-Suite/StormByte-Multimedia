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

#include <StormByte/buffer/io/buffered_location_reader.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/multimedia/attachment.hxx>
#include <StormByte/multimedia/container.hxx>
#include <StormByte/multimedia/metadata/file.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/stream.hxx>
#include <StormByte/multimedia/typedefs.hxx>

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <variant>

namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Multimedia module of the StormByte suite.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::FFmpeg
		 * @brief Private RAII wrappers over libav*.
		 */
		namespace FFmpeg {
			class AVFormatContext;
		}
	}
}

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Multimedia module of the StormByte suite.
	 */
	namespace Multimedia {
		/**
		 * @class File
		 * @brief Read-only consultation snapshot of a media source.
		 *
		 * File lists a locator, container, streams (HDR / HDR10+ and other
		 * mapped properties), attachments, metadata and duration. It has no
		 * write API and is not part of the tube. There is no public Reader().
		 *
		 * Open(path) creates a temporary local file leaf and drops it after
		 * probing so the handle is not held (Windows locking). Open(reader)
		 * probes any BufferedLocationReader with AVIO and keeps a borrowed
		 * reference; the caller retains ownership. Duration() uses that reader
		 * or builds another temporary local reader from the stored path.
		 * Path() returns the stored path or the borrowed reader locator.
		 *
		 * Open probes headers and a bounded run of video packets for HDR10+.
		 * It does not read the whole source for Duration.
		 *
		 * @see StormByte::Buffer::IO::BufferedLocationReader
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC File {
			public:
				File(const File&) = delete;

				/**
				 * @brief Move constructor.
				 * @param other File to take.
				 */
				File(File&& other) noexcept;

				/**
				 * @brief Destructor.
				 */
				~File() noexcept;

				File& operator=(const File&) = delete;
				File& operator=(File&&) = delete;

				/**
				 * @brief Path label: stored path, or reader.Path() if borrowed.
				 * @return Label.
				 */
				StormByte::Safe::String Path() const;

				/**
				 * @brief Detected container.
				 * @return Registry container.
				 */
				const class Container& Container() const noexcept { return m_container; }

				/**
				 * @brief Real streams in container order. Attached pictures are omitted.
				 * @return Immutable list.
				 */
				const Multimedia::Streams& Streams() const noexcept { return m_streams; }

				/**
				 * @brief Container attachments (covers, fonts). Not listed in Streams().
				 * @return Attachments captured at Open.
				 */
				const Multimedia::Attachments& Attachments() const noexcept;

				/**
				 * @brief Container-level tags captured at Open.
				 * @return Metadata snapshot.
				 */
				const Metadata::File& Metadata() const noexcept { return m_metadata; }

				/**
				 * @brief Container duration.
				 * @return Duration, or empty if it cannot be determined.
				 *
				 * Returns the header value, the duration passed to Open, or a value
				 * cached after the first scan. The first call may read the whole source
				 * when Open was used without a duration: even if the container header
				 * has a duration, streams that lack one are filled from packet
				 * timestamps. After a successful scan the result is reused on this
				 * instance. If Open(..., duration) was used, this is that value, there
				 * is no extra I/O, and stream durations stay as probed.
				 */
				const std::optional<Property::Duration>& Duration() const noexcept;

				/**
				 * @brief Opens and probes @p path. Temporary reader is dropped.
				 * @param path Media file.
				 * @param duration Authoritative duration; empty means scan on first Duration().
				 * @return Snapshot or FileOpenException.
				 */
				static ExpectedFile Open(const std::filesystem::path& path,
					std::optional<std::chrono::nanoseconds> duration = std::nullopt) noexcept;

				/**
				 * @brief Opens and probes @p reader with AVIO only.
				 * @param reader Existing origin. Not taken. Rewound before return.
				 * @param duration Authoritative duration; empty means scan on first Duration().
				 * @return Snapshot or FileOpenException.
				 */
				static ExpectedFile Open(StormByte::Buffer::IO::BufferedLocationReader& reader,
					std::optional<std::chrono::nanoseconds> duration = std::nullopt) noexcept;

			private:
				using Origin = std::variant<
					std::filesystem::path,
					std::reference_wrapper<StormByte::Buffer::IO::BufferedLocationReader>
				>;

				Origin m_origin;										///< Path or borrowed reader
				const class Container& m_container;						///< Registry container
				mutable Multimedia::Streams m_streams;					///< Probed streams
				Multimedia::Attachments m_attachments;					///< Covers / attached files
				Metadata::File m_metadata;								///< Container tags
				mutable std::optional<Property::Duration> m_duration;	///< Container duration
				mutable bool m_durationResolved;						///< Caller-supplied or scan done

				/**
				 * @brief Snapshot constructor.
				 * @param origin Stored path or borrowed reader.
				 * @param container Registry container.
				 * @param streams Probed streams.
				 * @param attachments Probed attachments.
				 * @param metadata Container tags.
				 * @param duration Container duration.
				 * @param durationResolved true if Duration() must not scan.
				 */
				File(Origin origin, const class Container& container,
					Multimedia::Streams streams, Multimedia::Attachments attachments,
					Metadata::File metadata,
					std::optional<Property::Duration> duration, bool durationResolved) noexcept;

				/**
				 * @brief Probe an already constructed reader (AVIO only).
				 * @param reader Origin used with AVIO.
				 * @param duration Caller-supplied duration, if any.
				 * @param origin Path to keep, or borrowed reference.
				 * @return Snapshot or FileOpenException.
				 */
				static ExpectedFile Probe(StormByte::Buffer::IO::BufferedLocationReader& reader,
					std::optional<std::chrono::nanoseconds> duration,
					Origin origin) noexcept;

				/**
				 * @brief Packet scan for container and missing stream durations.
				 */
				void ResolveDuration() const noexcept;

				/**
				 * @brief Opens AVIO on @p reader and scans durations.
				 * @param reader Origin.
				 * @param streams Streams to update.
				 * @param duration Container duration to fill if empty.
				 */
				static void ScanWithReader(StormByte::Buffer::IO::BufferedLocationReader& reader,
					Multimedia::Streams& streams,
					std::optional<Property::Duration>& duration) noexcept;

				/**
				 * @brief Sets HDR10+ on a video stream.
				 * @param stream Stream to update.
				 */
				static void MarkHdr10Plus(Stream& stream) noexcept;

				/**
				 * @brief Peeks video packets for HDR10+ side data.
				 * @param ctx Open probe context.
				 * @param streams Streams to mark.
				 */
				static void DetectHdr10Plus(FFmpeg::AVFormatContext& ctx, Multimedia::Streams& streams) noexcept;

				/**
				 * @brief Fills missing durations from packet timestamps.
				 * @param ctx Open probe context.
				 * @param streams Streams to update.
				 * @param container Container duration to fill if empty.
				 */
				static void ScanDurations(FFmpeg::AVFormatContext& ctx, Multimedia::Streams& streams,
					std::optional<Property::Duration>& container) noexcept;
		};
	}
}

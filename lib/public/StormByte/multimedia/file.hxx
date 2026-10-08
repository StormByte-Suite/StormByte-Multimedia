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
#include <StormByte/multimedia/attachment.hxx>
#include <StormByte/multimedia/container.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/metadata/file.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/stream.hxx>
#include <StormByte/multimedia/typedefs.hxx>
#include <StormByte/safe/function.hxx>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/unordered_map.hxx>
#include <StormByte/safe/vector.hxx>

#include <cstdint>

/**
 * @brief Multimedia-owned pipeline stages and unit holders.
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @brief Demuxer restoring codec metadata from the consultation snapshot.
	 */
	class Demuxer;
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
		 * @namespace StormByte::Multimedia::FFmpeg
		 * @brief Private RAII wrappers over libav*.
		 */
		namespace FFmpeg {
			/**
			 * @brief Open FFmpeg format context used by the probe.
			 */
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
		 * Base, Multimedia and registry providers must remain loaded while
		 * snapshots, borrowed readers and callback contexts are in use.
		 *
		 * @see StormByte::Buffer::IO::BufferedLocationReader
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC File {
			friend class StormByte::Multimedia::Backend::Pipeline::Demuxer;
			public:
				/**
				 * @brief Provider-owned observer receiving monotone scan percentages.
				 * @note Supply context, invoke, clone and release callbacks from the
				 *       provider module. Invocation is synchronous; no callback is retained.
				 *       Failed statuses and exceptions are ignored during notification.
				 */
				using DurationProgress = StormByte::Safe::Function<void(double)>;

				/**
				 * @brief Copy construction is unavailable for source snapshots.
				 * @param other Snapshot that cannot be copied.
				 */
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

				/**
				 * @brief Copy assignment is unavailable for source snapshots.
				 * @param other Snapshot that cannot be copied.
				 * @return This snapshot.
				 */
				File& operator=(const File& other) = delete;

				/**
				 * @brief Move assignment is unavailable because the container is borrowed.
				 * @param other Snapshot that cannot be assigned.
				 * @return This snapshot.
				 */
				File& operator=(File&& other) = delete;

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
				const StormByte::Safe::Vector<Stream>& Streams() const noexcept { return m_streams; }

				/**
				 * @brief Container attachments (covers, fonts). Not listed in Streams().
				 * @return Attachments captured at Open.
				 */
				const StormByte::Safe::Vector<Attachment>& Attachments() const noexcept;

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
				const StormByte::Safe::Optional<Property::Duration>& Duration() const noexcept;

				/**
				 * @brief Resolve duration while reporting packet scan progress.
				 * @param progress Byte-position observer; not called for a cached or supplied duration.
				 * @return Resolved duration, or empty if unavailable.
				 */
				const StormByte::Safe::Optional<Property::Duration>& Duration(const DurationProgress& progress) const noexcept;

				/**
				 * @brief Opens and probes @p path. Temporary reader is dropped.
				 * @param path UTF-8 media file path, copied into the snapshot.
				 * @param duration Authoritative nanoseconds; empty means scan on first Duration().
				 * @return Snapshot or FileOpenException.
				 */
				static ExpectedFile Open(const StormByte::Safe::String& path,
					StormByte::Safe::Optional<std::int64_t> duration = {}) noexcept;

				/**
				 * @brief Opens and probes @p reader with AVIO only.
				 * @param reader Existing origin. Not taken. Rewound before return.
				 * @param duration Authoritative nanoseconds; empty means scan on first Duration().
				 * @return Snapshot or FileOpenException.
				 */
				static ExpectedFile Open(StormByte::Buffer::IO::BufferedLocationReader& reader,
					StormByte::Safe::Optional<std::int64_t> duration = {}) noexcept;

			private:
				StormByte::Safe::String m_path;						///< Stored local path
				StormByte::Buffer::IO::BufferedLocationReader* m_reader;	///< Borrowed reader, or nullptr for a local path
				const class Container& m_container;						///< Registry container
				mutable StormByte::Safe::Vector<Stream> m_streams;			///< Probed streams
				StormByte::Safe::Vector<Attachment> m_attachments;			///< Covers / attached files
				Metadata::File m_metadata;								///< Container tags
				mutable StormByte::Safe::Optional<Property::Duration> m_duration;	///< Container duration
				mutable bool m_durationResolved;						///< Caller-supplied or scan done
				StormByte::Safe::UnorderedMap<int, StormByte::Safe::Shared<FFmpeg::AVCodecParameters>> m_codecParameters;	///< Probed codec parameters, including harvested HDR metadata.

				/**
				 * @brief Snapshot constructor.
				 * @param path Stored UTF-8 path for a local source.
				 * @param reader Borrowed reader, or nullptr for a local source.
				 * @param container Registry container.
				 * @param streams Probed streams.
				 * @param attachments Probed attachments.
				 * @param metadata Container tags.
				 * @param duration Container duration.
				 * @param durationResolved true if Duration() must not scan.
				 */
				File(StormByte::Safe::String path, StormByte::Buffer::IO::BufferedLocationReader* reader,
					const class Container& container,
					StormByte::Safe::Vector<Stream> streams, StormByte::Safe::Vector<Attachment> attachments,
					Metadata::File metadata,
					StormByte::Safe::Optional<Property::Duration> duration, bool durationResolved) noexcept;

				/**
				 * @brief Probe an already constructed reader (AVIO only).
				 * @param reader Origin used with AVIO.
				 * @param duration Caller-supplied duration, if any.
				 * @param path Stored UTF-8 path for a local source.
				 * @param borrowed Borrowed reader, or nullptr for a local source.
				 * @return Snapshot or FileOpenException.
				 */
				static ExpectedFile Probe(StormByte::Buffer::IO::BufferedLocationReader& reader,
					StormByte::Safe::Optional<std::int64_t> duration,
					StormByte::Safe::String path,
					StormByte::Buffer::IO::BufferedLocationReader* borrowed) noexcept;

				/**
				 * @brief Packet scan for container and missing stream durations.
				 * @param progress Borrowed byte-position observer, or nullptr.
				 */
				void ResolveDuration(const DurationProgress* progress) const noexcept;

				/**
				 * @brief Opens AVIO on @p reader and scans durations.
				 * @param reader Origin.
				 * @param streams Streams to update.
				 * @param duration Container duration to fill if empty.
				 * @param progress Borrowed packet scan observer, or nullptr.
				 */
				static void ScanWithReader(StormByte::Buffer::IO::BufferedLocationReader& reader,
					StormByte::Safe::Vector<Stream>& streams,
					StormByte::Safe::Optional<Property::Duration>& duration, const DurationProgress* progress) noexcept;

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
				static void DetectHdr10Plus(FFmpeg::AVFormatContext& ctx, StormByte::Safe::Vector<Stream>& streams) noexcept;

				/**
				 * @brief Fills missing durations from packet timestamps.
				 * @param ctx Open probe context.
				 * @param streams Streams to update.
				 * @param container Container duration to fill if empty.
				 * @param reader Borrowed source used to determine byte-based progress.
				 * @param progress Borrowed scan observer, or nullptr.
				 * @param percent Last published monotone percentage.
				 * @return true only if packet reading reached EOF rather than a read error.
				 */
				static bool ScanDurations(FFmpeg::AVFormatContext& ctx, StormByte::Safe::Vector<Stream>& streams,
					StormByte::Safe::Optional<Property::Duration>& container,
					StormByte::Buffer::IO::BufferedLocationReader& reader,
					const DurationProgress* progress, double& percent) noexcept;
		};
	}
}

/**
 * @brief Registers File snapshots with Multimedia-owned lifetime operations.
 * @note The private codec map is created, moved and destroyed in Multimedia.
 *       Borrowed readers, registry containers and provider modules must outlive use.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::File);

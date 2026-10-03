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
#include <StormByte/buffer/io/buffered_location_writer.hxx>
#include <StormByte/buffer/telemetry.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/multimedia/codec.hxx>
#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/pipeline/filters/report.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/progress.hxx>
#include <StormByte/multimedia/pipeline/telemetry.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/multimedia/typedefs.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/clonable.hxx>
#include <StormByte/type_traits.hxx>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	class Transcoder;	///< Coordinator behind @ref StormByte::Multimedia::Pipeline::Transcoder.
}

/**
 * @namespace StormByte::Multimedia::Pipeline
 * @brief Demux / decode / filter / encode / mux types.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Pipeline {
	class Encoder;
	class Transcoder;

	/**
	 * @enum Status
	 * @brief Lifecycle of a Transcoder instance.
	 *
	 * The constructor leaves the job Stopped. Run starts the coordinator.
	 * OnStart may return Stopped / Error / Aborted to bail without
	 * starting the job. A second Run fails. Pause does not require a
	 * new instance. After Stop the job is spent.
	 *
	 * @ingroup multimedia_pipeline
	 */
	enum class Status {
		Stopped,	///< Constructed; Run has not started, or OnStart declined
		Running,	///< Coordinator is alive and not paused
		Paused,		///< Pause(); coordinator blocks until Resume or Cancel
		Done,		///< Finished and flushed
		Error,		///< A stage failed; Error() has text
		Aborted		///< Cancel() while Running/Paused, or OnStart returned Aborted
	};

	/**
	 * @class TrackSettled
	 * @brief What an encoder actually opened. Not intention.
	 *
	 * Plan / Track are the request. This type is filled after Encoder
	 * is open and passed to Transcoder::OnSettled. Derive it and return
	 * the type from Transcoder::EmptySettled to carry extra fields.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC TrackSettled:
		public StormByte::Safe::Clonable<TrackSettled, StormByte::Safe::Unique<TrackSettled>> {
		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Empty settled row.
			 */
			TrackSettled() noexcept = default;

			/**
			 * @brief Copy constructor.
			 * @param other Source row.
			 */
			TrackSettled(const TrackSettled& other) = default;

			/**
			 * @brief Move constructor.
			 * @param other Row to take.
			 */
			TrackSettled(TrackSettled&& other) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			virtual ~TrackSettled() noexcept override = default;

			/**
			 * @brief Copy assignment.
			 * @param other Source row.
			 * @return *this.
			 */
			TrackSettled& operator=(const TrackSettled& other) = default;

			/**
			 * @brief Move assignment.
			 * @param other Row to take.
			 * @return *this.
			 */
			TrackSettled& operator=(TrackSettled&& other) noexcept = default;

			/**
			 * @}
			 */

			/**
			 * @brief Deep copy.
			 * @return Owning pointer to a new row of the same dynamic type.
			 */
			inline PointerType Clone() const override {
				return MakePointer<TrackSettled>(*this);
			}

			/**
			 * @brief Move into a new pointer.
			 * @return Owning pointer to the moved row.
			 */
			inline PointerType Move() override {
				return MakePointer<TrackSettled>(std::move(*this));
			}

			/**
			 * @brief Human-readable line for logs.
			 * @return One or more lines, no trailing newline required.
			 */
			virtual std::string ToString() const;

			int In = -1;												///< Origin stream or attachment slot
			int Out = -1;												///< Index in Plan::Tracks after Add
			Type Kind = Type::Unknown;									///< Media kind
			const Codec* Source = nullptr;								///< Origin codec
			const Codec* Destination = nullptr;							///< Opened encoder codec, or nullptr if remux
			std::optional<std::string> Implementation;					///< Opened encoder pin
			std::optional<int> Crf;										///< CRF/CQ actually used
			std::optional<std::int64_t> BitRate;						///< Bitrate actually used
			std::optional<std::int64_t> MaxBitRate;						///< VBV actually used
			std::optional<std::string> Preset;							///< Preset actually used
			std::optional<std::string> Tune;							///< Tune actually used
			std::map<std::string, std::string> FineTune;				///< Vendor leftovers actually used
			std::optional<int> SampleFormat;							///< Encoder AVSampleFormat
			std::optional<int> SourceChannels;							///< Decoded channel count
			std::optional<int> EncoderChannels;							///< Encoder channel count
			std::optional<int> FrameSize;								///< Encoder frame_size
			std::optional<int> SampleRate;								///< Samples per second
	};

	/**
	 * @class Transcoder
	 * @brief Facade that maps tracks and runs one reader-to-writer job.
	 *
	 * Connects operator>> and Filters for you. The stock class is a
	 * complete job: own reader/writer locations, choose origin streams,
	 * remux or encode each one, write through the Plan writer. You do
	 * not have to derive anything to transcode.
	 *
	 * Four input/output combinations are accepted: path/path, path/writer,
	 * reader/path, and reader/writer. Path arguments create local
	 * BufferedFile leaves; location arguments are Safe::Unique owners and
	 * preserve their dynamic type. Run transfers both owners to EmptyPlan,
	 * after which the Transcoder no longer owns them. File is only used as
	 * a consultation snapshot and is discarded after analysis.
	 * File::Reader is not used.
	 *
	 * It is also the extension point. Mix what you need:
	 *
	 * - Hooks only. Keep the stock Plan and override OnConfigure,
	 *   OnStart, OnPlan, OnSettled, OnMeasureDone, OnAnalyticsDone,
	 *   OnProgress, OnDone, OnError, OnAborted.
	 * - A richer intention. Override EmptyPlan and return a type
	 *   derived from Plan. Plan::Check is virtual on that type.
	 * - A richer settled row. Override EmptySettled and return a
	 *   type derived from TrackSettled.
	 * - Logging. Override @ref InstallLog so this job's own lines
	 *   use another component path. Tube stages keep the Multimedia
	 *   defaults.
	 *
	 * EmptyPlan / EmptySettled only pick the dynamic type. This class
	 * still fills tracks from the fluent map and settled fields from
	 * the opened Encoder. One instance is one Run. Another job is
	 * another instance.
	 *
	 * Mux order is the order of Video / Audio / Subtitle / Attachments
	 * calls. There is no output-index argument and no Destination
	 * setter: the container is the writer extension.
	 *
	 * Analytics attach with @ref Filter. After the job reaches
	 * Status::Done, @ref Reports returns the same snapshots a hand
	 * tube reads with @ref Filter::Analytics::Report on the leaf
	 * pointer.
	 *
	 * @ref Progress forwards the Demuxer clock. The user may keep
	 * that shared_ptr after the tube dies.
	 *
	 * Each constructor calls @ref InstallLog after the most-derived
	 * constructor body of this class. A derived constructor that
	 * constructs a subclass must call @ref InstallLog itself after
	 * that constructor.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Transcoder {
		public:
			/**
			 * @class Track
			 * @brief Fluent handle for one origin stream in this job.
			 *
			 * Writes the matching Config leaf. Mux order is the order
			 * of Add on this job, not a field here. Call before Run.
			 *
			 * @ingroup multimedia_pipeline
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Track {
				public:
					/**
					 * @name Lifecycle
					 * @{
					 */

					/**
					 * @brief Copy constructor.
					 * @param other Source handle.
					 */
					Track(const Track& other) noexcept = default;

					/**
					 * @brief Move constructor.
					 * @param other Handle to take.
					 */
					Track(Track&& other) noexcept = default;

					/**
					 * @brief Destructor.
					 */
					~Track() noexcept = default;

					/**
					 * @brief Copy assignment.
					 * @param other Source handle.
					 * @return *this.
					 */
					Track& operator=(const Track& other) noexcept = default;

					/**
					 * @brief Move assignment.
					 * @param other Handle to take.
					 * @return *this.
					 */
					Track& operator=(Track&& other) noexcept = default;

					/**
					 * @}
					 */

					/**
					 * @brief Remux this track (no destination codec).
					 * @return *this.
					 */
					Track& Remux() noexcept;

					/**
					 * @brief Encodes this track to @p codec.
					 * @param codec Destination registry codec.
					 * @return *this.
					 */
					Track& Codec(const Codec& codec) noexcept;

					/**
					 * @brief Pins an FFmpeg encoder name.
					 * @param name Table name.
					 * @return *this.
					 */
					Track& Implementation(std::string name) noexcept;

					/**
					 * @brief Sets CRF/CQ. Clears BitRate.
					 * @param value Quality value.
					 * @return *this.
					 */
					Track& CRF(int value) noexcept;

					/**
					 * @brief Sets target bitrate. Clears CRF.
					 * @param bits_per_second Bits per second.
					 * @return *this.
					 */
					Track& BitRate(std::int64_t bits_per_second) noexcept;

					/**
					 * @brief Sets VBV / max bitrate.
					 * @param bits_per_second Bits per second.
					 * @return *this.
					 */
					Track& MaxBitRate(std::int64_t bits_per_second) noexcept;

					/**
					 * @brief Sets encoder preset.
					 * @param name Preset name.
					 * @return *this.
					 */
					Track& Preset(std::string name) noexcept;

					/**
					 * @brief Sets encoder tune.
					 * @param name Tune name.
					 * @return *this.
					 */
					Track& Tune(std::string name) noexcept;

					/**
					 * @brief Replaces vendor leftovers.
					 * @param options Key / value map.
					 * @return *this.
					 */
					Track& FineTune(std::map<std::string, std::string> options) noexcept;

					/**
					 * @brief Overrides stream language.
					 * @param language ISO tag. Empty clears the override.
					 * @return *this.
					 */
					Track& Language(std::string language) noexcept;

					/**
					 * @brief Overrides stream title.
					 * @param title Title text. Empty clears the override.
					 * @return *this.
					 */
					Track& Title(std::string title) noexcept;

					/**
					 * @brief Appends a Process, Packet or Analytics filter to this track.
					 * @tparam FilterType Child of Filter::Process, Filter::Packet
					 *         or Filter::Analytics.
					 * @param args Constructor arguments, forwarded.
					 * @return *this.
					 *
					 * Call before Transcoder::Run. Frame filters on a remux
					 * track Fail the stretch. Global analytics attach on
					 * Transcoder::Filter. Both are allowed; there is no dedup.
					 */
					template<typename FilterType, typename... Args>
					Track& Filter(Args&&... args) noexcept {
						m_owner->AttachFilter(m_slot,
							std::make_shared<FilterType>(std::forward<Args>(args)...));
						return *this;
					}

				private:
					friend class Transcoder;

					/**
					 * @brief Binds this handle to a mapped slot.
					 * @param owner Parent job.
					 * @param slot Index into the job map.
					 */
					Track(Transcoder& owner, std::size_t slot) noexcept;

					Transcoder* m_owner;								///< Parent job
					std::size_t m_slot;									///< Job map index
			};

			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Builds reader and writer from paths.
			 * @param source Input path.
			 * @param destination Output path.
			 * @param logger Shared log for the job and the tube.
			 * @param duration Authoritative source duration. When omitted, the source is scanned before pipeline stages start.
			 *
			 * Stores @p logger as-is. @ref InstallLog runs at the end
			 * of this constructor.
			 */
			Transcoder(const std::filesystem::path& source,
				const std::filesystem::path& destination,
				std::shared_ptr<StormByte::Logger::Log> logger,
				std::optional<std::chrono::nanoseconds> duration = std::nullopt) noexcept;

			/**
			 * @brief Builds a local reader and takes @p writer.
			 * @param source Input path.
			 * @param writer Owned output location (moved).
			 * @param logger Shared log for the job and the tube.
			 * @param duration Authoritative source duration. When omitted, the source is scanned before pipeline stages start.
			 */
			Transcoder(const std::filesystem::path& source,
				StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
				std::shared_ptr<StormByte::Logger::Log> logger,
				std::optional<std::chrono::nanoseconds> duration = std::nullopt) noexcept;

			/**
			 * @brief Takes @p reader and builds a local writer.
			 * @param reader Owned input location (moved).
			 * @param destination Output path.
			 * @param logger Shared log for the job and the tube.
			 * @param duration Authoritative source duration. When omitted, the source is scanned before pipeline stages start.
			 */
			Transcoder(StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
				const std::filesystem::path& destination,
				std::shared_ptr<StormByte::Logger::Log> logger,
				std::optional<std::chrono::nanoseconds> duration = std::nullopt) noexcept;

			/**
			 * @brief Takes both location owners without slicing.
			 * @param reader Owned input location (moved).
			 * @param writer Owned output location (moved).
			 * @param logger Shared log for the job and the tube.
			 * @param duration Authoritative source duration. When omitted, the source is scanned before pipeline stages start.
			 */
			Transcoder(StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
				StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
				std::shared_ptr<StormByte::Logger::Log> logger,
				std::optional<std::chrono::nanoseconds> duration = std::nullopt) noexcept;

			/**
			 * @brief Copy constructor.
			 * @param other Source job.
			 */
			Transcoder(const Transcoder& other) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Job to take.
			 */
			Transcoder(Transcoder&& other) noexcept = delete;

			/**
			 * @brief Destructor. Stops the coordinator and joins.
			 */
			virtual ~Transcoder() noexcept;

			/**
			 * @brief Copy assignment.
			 * @param other Source job.
			 * @return *this.
			 */
			Transcoder& operator=(const Transcoder& other) = delete;

			/**
			 * @brief Move assignment.
			 * @param other Job to take.
			 * @return *this.
			 */
			Transcoder& operator=(Transcoder&& other) noexcept = delete;

			/**
			 * @}
			 */

			/**
			 * @brief Logger used by this job after @ref InstallLog.
			 * @return Job facade, or empty.
			 *
			 * Stock path is StormByte/Multimedia/Transcoder. Do not
			 * pass this pointer to a Step or filter; those use the
			 * application logger and the Multimedia stage path.
			 */
			const std::shared_ptr<StormByte::Logger::Log>& Logger() const noexcept;

			/**
			 * @brief Intention built for this job, if any.
			 * @return Plan, or empty before Run.
			 */
			inline const std::shared_ptr<class Plan>& Plan() const noexcept {
				return m_plan;
			}

			/**
			 * @name Map
			 * @{
			 */

			/**
			 * @brief Maps a video origin stream. Mux order is Add order.
			 * @param in Origin stream index.
			 * @return Fluent handle.
			 */
			Track Video(int in) noexcept;

			/**
			 * @brief Maps an audio origin stream. Mux order is Add order.
			 * @param in Origin stream index.
			 * @return Fluent handle.
			 */
			Track Audio(int in) noexcept;

			/**
			 * @brief Maps a subtitle origin stream. Mux order is Add order.
			 * @param in Origin stream index.
			 * @return Fluent handle.
			 */
			Track Subtitle(int in) noexcept;

			/**
			 * @brief Keeps every source attachment.
			 * @return *this.
			 *
			 * Alias of Attachments with the star-star MIME pattern.
			 * Default is drop (do not call this).
			 */
			Transcoder& Attachments() noexcept;

			/**
			 * @brief Keeps source attachments whose MIME matches @p pattern.
			 * @param pattern Exact type/subtype, type-star category, or
			 *        star-star (all). Same matcher as Plan::Check.
			 * @return *this.
			 */
			Transcoder& Attachments(std::string_view pattern) noexcept;

			/**
			 * @brief Drops an origin stream (omit from the Plan).
			 * @param in Origin stream index.
			 * @return *this.
			 */
			Transcoder& Ignore(int in) noexcept;

			/**
			 * @brief Appends a global analytics filter. One node, every matching stretch.
			 * @tparam FilterType Child of Filter::Analytics.
			 * @param args Constructor arguments, forwarded.
			 * @return *this.
			 *
			 * Track-scoped analytics attach on Track::Filter. Both
			 * are allowed; there is no dedup.
			 */
			template<typename FilterType, typename... Args>
			Transcoder& Filter(Args&&... args) noexcept {
				static_assert(std::is_base_of_v<Filter::Analytics, FilterType>,
					"Track filters attach on Track::Filter");
				AttachAnalytics(std::make_shared<FilterType>(std::forward<Args>(args)...));
				return *this;
			}

			/**
			 * @}
			 */

			/**
			 * @name Run
			 * @{
			 */

			/**
			 * @brief Builds the Plan, starts the coordinator and returns.
			 *
			 * A second Run fails. Calls OnConfigure, EmptyPlan (moves the
			 * BufferedFile leaves), fills tracks from the fluent map,
			 * OnPlan, OnStart, then plan >> demuxer >> muxer. Does not
			 * block until Done.
			 */
			void Run() noexcept;

			/**
			 * @brief Requests abort. Coordinator ends in Status::Aborted.
			 */
			void Cancel() noexcept;

			/**
			 * @brief Pauses the coordinator.
			 */
			void Pause() noexcept;

			/**
			 * @brief Resumes after Pause.
			 */
			void Resume() noexcept;

			/**
			 * @brief Current lifecycle value.
			 * @return Status.
			 */
			enum Status Status() const noexcept;

			/**
			 * @brief Whether the job failed.
			 * @return true after Status::Error.
			 */
			bool Failed() const noexcept;

			/**
			 * @brief Failure text.
			 * @return Message, or empty.
			 */
			std::optional<std::string> Error() const noexcept;

			/**
			 * @brief Shared telemetry for the original input reader.
			 * @return Shared handle. Retain it to inspect final I/O counters after this job dies.
			 */
			StormByte::Safe::Shared<StormByte::Buffer::ReadTelemetry> InputTelemetry() const noexcept;

			/**
			 * @brief Shared telemetry for the destination writer.
			 * @return Shared handle. Retain it to inspect final I/O counters after this job dies.
			 */
			StormByte::Safe::Shared<StormByte::Buffer::WriteTelemetry> OutputTelemetry() const noexcept;

			/**
			 * @brief Shared tube clock. Forwards Demuxer::Progress().
			 *
			 * Empty before Run wires the Demuxer. The user may keep
			 * the pointer after the job dies. No public setters.
			 *
			 * @return Const shared handle, or empty.
			 */
			Progress::Pointer Progress() const noexcept;

			/**
			 * @brief Analytics snapshots after the job is Idle.
			 * @return Pair of flattened key (`vmaf[0]`, `vmaf[general]`,
			 *         `vmaf[0]#2` on an exact-key repeat) and
			 *         @ref Filter::Report.
			 *
			 * Track-scoped leaves use the origin index. Global Add
			 * uses `general`. Same contract as
			 * @ref Filters::Reports. Snapshots only; the leaves stay
			 * owned by the job. Meaningful after Status::Done.
			 * A low score is still Ok. Notice lines from a leaf
			 * are log, not this API.
			 */
			std::vector<std::pair<std::string, Filter::Report>> Reports() const noexcept;

			/**
			 * @brief Shared job and stage telemetry, available before Run.
			 *
			 * Retain the handle to inspect final counters after the worker
			 * and its pipeline stages have been destroyed. Resident memory
			 * values are sampled process RSS and are not per-stage allocations.
			 *
			 * @return Shared telemetry handle, or empty without a backend.
			 */
			std::shared_ptr<const JobTelemetry> Telemetry() const noexcept;

			/**
			 * @brief true if not failed.
			 * @return Not Failed.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @}
			 */

		protected:
			/**
			 * @brief Scopes this job's own logger.
			 *
			 * Stock path is StormByte/Multimedia/Transcoder with the
			 * Multimedia format and throttle. Override to use another
			 * path and other rules. Tube stages still use
			 * StormByte/Multimedia/<stage>. Do not call from a
			 * constructor. Each stock constructor calls this after
			 * the most-derived constructor of this class.
			 */
			virtual void InstallLog() noexcept;

			/**
			 * @brief Application logger passed to the constructor.
			 * @return Logger given to the constructor, not the job facade.
			 *
			 * Hand this to a Step or filter. Those apply the Multimedia
			 * stage path themselves.
			 */
			inline const std::shared_ptr<StormByte::Logger::Log>& ApplicationLog() const noexcept {
				return m_app_log;
			}

			/**
			 * @brief Allocates the Plan type for this job.
			 * @param reader Owned origin location, transferred from this Transcoder.
			 * @param writer Owned destination location, transferred from this Transcoder.
			 * @return Plan of the desired dynamic type, with no tracks yet.
			 *
			 * Override to return a type derived from Plan. Tracks are
			 * filled from the fluent map after this returns. After the
			 * call this Transcoder no longer owns the leaves.
			 */
			virtual std::unique_ptr<class Plan> EmptyPlan(
				StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
				StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
				std::optional<std::chrono::nanoseconds> duration = std::nullopt) const noexcept;

			/**
			 * @brief Allocates the settled-row type.
			 * @return Empty row of the desired dynamic type.
			 *
			 * Override to return a type derived from TrackSettled.
			 * MarkSettled fills it and calls OnSettled.
			 */
			virtual StormByte::Safe::Unique<TrackSettled> EmptySettled() const noexcept;

			/**
			 * @brief Last chance to raise ceilings before the job starts.
			 */
			virtual void OnConfigure() noexcept;

			/**
			 * @brief Gate after the Plan is filled.
			 * @return Running to proceed, Error / Aborted / Stopped to bail.
			 */
			virtual enum Status OnStart() noexcept;

			/**
			 * @brief Intention, just before plan >> demuxer.
			 * @param plan Filled Plan (still owned by this job).
			 */
			virtual void OnPlan(const class Plan& plan) noexcept;

			/**
			 * @brief One encode lane finished Encoder open.
			 * @param track Settled row from EmptySettled.
			 */
			virtual void OnSettled(const TrackSettled& track) noexcept;

			/**
			 * @brief Measure pass closed.
			 *
			 * Not called if this tube did not mount a measure pass.
			 */
			virtual void OnMeasureDone() noexcept;

			/**
			 * @brief Analytics taps idle.
			 *
			 * Not called if this tube did not mount analytics.
			 */
			virtual void OnAnalyticsDone() noexcept;

			/**
			 * @brief Progress tick. Read @ref Progress.
			 */
			virtual void OnProgress() noexcept;

			/**
			 * @brief Successful flush.
			 */
			virtual void OnDone() noexcept;

			/**
			 * @brief Hard error.
			 * @param message Error text.
			 */
			virtual void OnError(const std::string& message) noexcept;

			/**
			 * @brief Cancel completed.
			 */
			virtual void OnAborted() noexcept;

		private:
			friend class Backend::Pipeline::Transcoder;
			friend class Track;

			/**
			 * @brief Takes already heap-allocated leaves.
			 * @param reader Owned origin.
			 * @param writer Owned sink.
			 * @param logger Shared log.
			 */
			/**
			 * @brief Marks a hard error and cancels the coordinator.
			 * @param reason Message stored in Error().
			 */
			void Fail(std::string reason) noexcept;

			/**
			 * @brief Opens a consultation File from the reader path and discards it after use.
			 * @return false if owner.Fail() was called.
			 */
			bool ProbeSource() noexcept;

			/**
			 * @brief Maps one origin stream. Mux slot is the next Add.
			 * @param in Origin index.
			 * @param kind Expected type.
			 * @return Fluent track handle.
			 */
			Track AddTrack(int in, Type kind) noexcept;

			/**
			 * @brief Records encoder open into a TrackSettled and fires OnSettled.
			 * @param in Origin stream index of the lane.
			 * @param encoder Encoder that just opened.
			 */
			void MarkSettled(int in, Encoder& encoder) noexcept;

			/**
			 * @brief Whether @p slot is a mapped track.
			 * @param slot Job map index.
			 * @return true if the slot exists.
			 */
			bool ValidSlot(std::size_t slot) const noexcept;

			/**
			 * @brief Appends a track filter (Process, Packet or Analytics) to @p slot.
			 * @param slot Job map index.
			 * @param filter Filter instance.
			 */
			void AttachFilter(std::size_t slot, std::shared_ptr<Filter::FFmpeg> filter) noexcept;

			/**
			 * @brief Appends a global analytics filter (one node).
			 * @param filter Filter instance.
			 */
			void AttachAnalytics(std::shared_ptr<Filter::FFmpeg> filter) noexcept;

			std::shared_ptr<StormByte::Logger::Log> m_app_log;			///< Logger from the constructor; input for tube stages
			std::shared_ptr<StormByte::Logger::Log> m_logger;			///< Job facade after InstallLog
			StormByte::Safe::Shared<StormByte::Buffer::ReadTelemetry> m_input_telemetry;	///< Input counters retained across owner transfer
			StormByte::Safe::Shared<StormByte::Buffer::WriteTelemetry> m_output_telemetry;	///< Output counters retained across owner transfer
			StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> m_reader;	///< Origin until EmptyPlan
			StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> m_writer;	///< Sink until EmptyPlan
			std::optional<std::chrono::nanoseconds> m_duration;								///< Authoritative source duration, when supplied
			std::unique_ptr<File> m_consult;							///< Consultation snapshot; discarded after analysis
			std::shared_ptr<class Plan> m_plan;							///< Intention; shared with the job after Run
			std::unique_ptr<Backend::Pipeline::Transcoder> m_backend;	///< Map and coordinator thread
			bool m_armed;												///< EmptyPlan already consumed the leaves
	};
}

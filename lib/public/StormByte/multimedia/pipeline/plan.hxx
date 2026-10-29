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

#pragma once

#include <StormByte/bitmask.hxx>
#include <StormByte/buffer/io/buffered_location_reader.hxx>
#include <StormByte/buffer/io/buffered_location_writer.hxx>
#include <StormByte/buffer/telemetry.hxx>
#include <StormByte/multimedia/container.hxx>
#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/pipeline/typedefs.hxx>
#include <StormByte/multimedia/pipeline/resource_usage.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/atomic.hxx>
#include <StormByte/safe/function.hxx>
#include <StormByte/safe/mutex.hxx>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

#include <chrono>
#include <cstdint>
#include <utility>

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
		 * @namespace Backend::Pipeline
		 * @brief Internal pipeline consumers of the plan intention.
		 */
		namespace Backend::Pipeline {
			/**
			 * @brief Backend source restoration and track routing.
			 */
			class Demuxer;

			/**
			 * @brief Per-track queue capacity calculation.
			 */
			class Ceiling;

			/**
			 * @namespace Detail::Muxer::FFmpeg
			 * @brief Output format implementation.
			 */
			namespace Detail::Muxer::FFmpeg {
				/**
				 * @brief Writes the resolved plan container.
				 */
				class Container;
			}

			/**
			 * @brief Backend coordinator publishing execution state.
			 */
			class Transcoder;
		}

		/**
		 * @namespace StormByte::Multimedia::Pipeline
		 * @brief Demux / decode / filter / encode / mux types.
		 *
		 * @ingroup multimedia_pipeline
		 */
		namespace Pipeline {
				/**
				 * @brief Demux stage receiving the job intention.
				 */
			class Demuxer;

			/**
			 * @brief Decoder consuming the source snapshot.
			 */
			class Decoder;

			/**
			 * @brief Encoder consuming destination tracks.
			 */
			class Encoder;

			/**
			 * @brief Remux stage consuming the source snapshot.
			 */
			class Remuxer;

			/**
			 * @brief Output stage consuming the owned writer and container.
			 */
			class Muxer;

			/**
			 * @brief Filter facade consulting the source snapshot.
			 */
			class Filters;

			/**
			 * @brief Pipeline stage observing the shared plan state.
			 */
			class Step;

			/**
			 * @brief Private route for filter chains.
			 */
			class Route;

			/**
			 * @brief Public job facade publishing failure and cancellation.
			 */
			class Transcoder;

			/**
			 * @class Plan
			 * @brief Closed job intention: owned location reader, writer and tracks.
			 *
			 * Octets are held as @ref StormByte::Safe::Unique of the location
			 * bases. Paths build local BufferedFile leaves; supplied owners retain
			 * their dynamic types without slicing. The constructor probes the
			 * reader once into a consultation File snapshot
			 * (streams, attachments, metadata). That File is not the
			 * octet origin. Progress uses a positive explicit duration or the
			 * container estimate. Only when neither is usable is the full
			 * source scanned before pipeline stages are created. Explicit
			 * File::Duration() queries retain their full-scan behavior.
			 *
			 * Destination container is an owned copy of an explicit identity,
			 * or the registry identity matched to FFmpeg's filename guess.
			 * Explicit identities ignore the filename when selecting the muxer.
			 * Stages retain this noncopyable, nonmovable intention exclusively
			 * through @ref StormByte::Safe::Shared.
			 *
			 * Mux output order is the order of @ref add. `add` is not
			 * idempotent. A track omitted from @ref Tracks is dropped.
			 *
			 * @ref Check tests formation of this intention only. Success
			 * does not mean the tube will run. Boolean conversion reports
			 * whether @ref Check succeeds. @ref Policy::EnforceFeatures,
			 * set on construction, makes @ref Check reject an encode that
			 * would drop source features. @ref Policies(Policy) adds a
			 * flag. @ref Policies with `~flag` clears that flag. Neither
			 * call replaces the rest of the mask.
			 *
			 * @ingroup multimedia_pipeline
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Plan {
				public:
					/**
					 * @enum State
					 * @brief Shared execution state of the pipeline plan.
					 */
					enum class State: std::uint8_t {
						Created,	///< Constructed and not yet running.
						Running,	///< Pipeline execution has started.
						Finished,	///< Pipeline execution completed successfully.
						Failed		///< Pipeline formation or execution failed.
					};

					/**
					 * @enum Policy
					 * @brief Job-intention flags. Bits must be disjoint; Has() is a bitmask test.
					 *
					 * None is zero and is not a policy. EnforceFeatures is the
					 * first flag. Further policies take the next free bit.
					 */
					enum class Policy: std::uint32_t {
						None				= 0,		///< No flags.
						EnforceFeatures		= 1u << 0	///< Check rejects an encode that drops source features.
					};

					/**
					 * @brief Bitwise complement of one flag, for @ref Policies(Policy).
					 * @param policy Flag to negate.
					 * @return Policy whose defined bits are inverted.
					 *
					 * `Policies(~Policy::EnforceFeatures)` clears that flag
					 * and leaves every other flag as it was.
					 */
					friend constexpr enum Policy operator~(enum Policy policy) noexcept {
						return static_cast<enum Policy>(~static_cast<std::uint32_t>(policy));
					}

					/**
					 * @brief Converts a Policy to a stable token.
					 * @param policy Policy value.
					 * @return Literal. Unknown bits return "UnknownPolicy".
					 */
					static constexpr const char* ToString(enum Policy policy) noexcept {
						switch (policy) {
							case Policy::None:				return "None";
							case Policy::EnforceFeatures:	return "EnforceFeatures";
						}
						return "UnknownPolicy";
					}

					/**
					 * @class Policies
					 * @brief Bitmask of @ref Policy flags.
					 * @note Stores only enum flags and a Bitmask vtable, with no owning heap state.
					 * Conditional DLL safety requires a compatible compiler, standard-library ABI
					 * and provider layout, with the multimedia provider loaded during use.
					 */
					class STORMBYTE_MULTIMEDIA_PUBLIC Policies: public StormByte::Bitmask<Policies, Policy> {
						public:
							/**
							 * @brief Empty mask.
							 */
							constexpr Policies() noexcept: Bitmask() {}

							/**
							 * @brief Mask from one flag.
							 * @param policy Initial flag.
							 */
							constexpr Policies(enum Policy policy) noexcept: Bitmask(policy) {}

							/**
							 * @brief Copy constructor.
							 * @param other Source mask.
							 */
							constexpr Policies(const Policies& other) noexcept = default;

							/**
							 * @brief Move constructor.
							 * @param other Source mask.
							 */
							constexpr Policies(Policies&& other) noexcept = default;

							/**
							 * @brief Destructor.
							 */
							constexpr ~Policies() noexcept override = default;

							/**
							 * @brief Copy assignment.
							 * @param other Source mask.
							 * @return *this.
							 */
							constexpr Policies& operator=(const Policies& other) noexcept = default;

							/**
							 * @brief Move assignment.
							 * @param other Source mask.
							 * @return *this.
							 */
							constexpr Policies& operator=(Policies&& other) noexcept = default;
					};

					/**
					 * @brief Reads the synchronized execution state.
					 * @return Current plan state.
					 */
					enum State State() const noexcept;

					/**
					 * @brief Copies the synchronized first failure reason.
					 * @return Failure message, if one has been provided.
					 */
					StormByte::Safe::Optional<StormByte::Safe::String> Error() const noexcept;

					/**
					 * @name Lifecycle
					 * @{
					 */

					/**
					 * @brief Builds reader and writer from paths.
					 * @param source Input path.
					 * @param destination Output path.
					 * @param duration Authoritative nanoseconds; otherwise use the container estimate, scanning only if unavailable.
					 */
					Plan(const StormByte::Safe::String& source,
						const StormByte::Safe::String& destination,
						StormByte::Safe::Optional<std::int64_t> duration = {}) noexcept;

					/**
					 * @brief Builds a local reader and takes @p writer.
					 * @param source Input path.
					 * @param writer Owned output location (moved).
					 * @param duration Authoritative nanoseconds; otherwise use the container estimate, scanning only if unavailable.
					 */
					Plan(const StormByte::Safe::String& source,
						StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
						StormByte::Safe::Optional<std::int64_t> duration = {}) noexcept;

					/**
					 * @brief Takes @p reader and builds a local writer.
					 * @param reader Owned input location (moved).
					 * @param destination Output path.
					 * @param duration Authoritative nanoseconds; otherwise use the container estimate, scanning only if unavailable.
					 */
					Plan(StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
						const StormByte::Safe::String& destination,
						StormByte::Safe::Optional<std::int64_t> duration = {}) noexcept;

					/**
					 * @brief Takes both location owners without slicing.
					 * @param reader Owned input location (moved).
					 * @param writer Owned output location (moved).
					 * @param duration Authoritative nanoseconds; otherwise use the container estimate, scanning only if unavailable.
					 */
					Plan(StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
						StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
						StormByte::Safe::Optional<std::int64_t> duration = {}) noexcept;

					/**
					 * @brief Takes both owners and observes automatic duration resolution.
					 * @param reader Owned input location.
					 * @param writer Owned output location.
					 * @param duration Authoritative nanoseconds; otherwise use the container estimate, scanning only if unavailable.
					 * @param progress Borrowed observer for this call only; null disables updates.
					 */
					Plan(StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
						StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
						StormByte::Safe::Optional<std::int64_t> duration,
						const StormByte::Safe::Function<void(double)>* progress) noexcept;

					/**
					 * @brief Builds local locations with an owned explicit container copy.
					 * @param source Input path.
					 * @param destination Output path; its suffix does not select the format.
					 * @param container Registered writable destination identity to copy.
					 * @param duration Authoritative nanoseconds; otherwise use the container estimate, scanning only if unavailable.
					 */
					Plan(const StormByte::Safe::String& source,
						const StormByte::Safe::String& destination,
						const class Container& container,
						StormByte::Safe::Optional<std::int64_t> duration = {}) noexcept;

					/**
					 * @brief Builds a local reader and owns the writer and a container copy.
					 * @param source Input path.
					 * @param writer Owned output location.
					 * @param container Registered writable destination identity to copy.
					 * @param duration Authoritative nanoseconds; otherwise use the container estimate, scanning only if unavailable.
					 */
					Plan(const StormByte::Safe::String& source,
						StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
						const class Container& container,
						StormByte::Safe::Optional<std::int64_t> duration = {}) noexcept;

					/**
					 * @brief Owns the reader and a container copy and builds a local writer.
					 * @param reader Owned input location.
					 * @param destination Output path; its suffix does not select the format.
					 * @param container Registered writable destination identity to copy.
					 * @param duration Authoritative nanoseconds; otherwise use the container estimate, scanning only if unavailable.
					 */
					Plan(StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
						const StormByte::Safe::String& destination,
						const class Container& container,
						StormByte::Safe::Optional<std::int64_t> duration = {}) noexcept;

					/**
					 * @brief Owns both locations and a copy of the explicit destination identity.
					 * @param reader Owned input location.
					 * @param writer Owned output location.
					 * @param container Registered writable destination identity to copy.
					 * @param duration Authoritative nanoseconds; otherwise use the container estimate, scanning only if unavailable.
					 */
					Plan(StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
						StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
						const class Container& container,
						StormByte::Safe::Optional<std::int64_t> duration = {}) noexcept;

					/**
					 * @brief Owns locations and a container copy while observing duration resolution.
					 * @param reader Owned input location.
					 * @param writer Owned output location.
					 * @param container Registered writable destination identity to copy.
					 * @param duration Authoritative nanoseconds; otherwise use the container estimate, scanning only if unavailable.
					 * @param progress Borrowed duration observer; null disables updates.
					 */
					Plan(StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
						StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
						const class Container& container,
						StormByte::Safe::Optional<std::int64_t> duration,
						const StormByte::Safe::Function<void(double)>* progress) noexcept;

					/**
					 * @brief Copy construction is not supported.
					 * @param other Source plan.
					 */
					Plan(const Plan& other) = delete;

					/**
					 * @brief Move construction is not supported; stages retain shared ownership.
					 * @param other Source plan.
					 */
					Plan(Plan&& other) noexcept = delete;

					/**
					 * @brief Destructor.
					 */
					virtual ~Plan() noexcept;

					/**
					 * @brief Copy assignment is not supported.
					 * @param other Source plan.
					 * @return This plan.
					 */
					Plan& operator=(const Plan& other) = delete;

					/**
					 * @brief Move assignment is not supported; stages retain shared ownership.
					 * @param other Source plan.
					 * @return This plan.
					 */
					Plan& operator=(Plan&& other) noexcept = delete;

					/**
					 * @}
					 */

					/**
					 * @brief Same as @ref Check having a value.
					 * @return true if this intention is well formed.
					 */
					explicit operator bool() const noexcept;

					/**
					 * @name Job
					 * @{
					 */

					/**
					 * @brief Shared telemetry for the owned input reader.
					 * @return Shared handle. Retain it to inspect final I/O counters after this Plan dies.
					 */
					StormByte::Safe::Shared<StormByte::Buffer::ReadTelemetry> InputTelemetry() const noexcept;

					/**
					 * @brief Shared telemetry for the owned output writer.
					 * @return Shared handle. Retain it to inspect final I/O counters after this Plan dies.
					 */
					StormByte::Safe::Shared<StormByte::Buffer::WriteTelemetry> OutputTelemetry() const noexcept;

					/**
					 * @brief Appends a clone of @p track. Not idempotent.
					 * @param track Track to copy.
					 */
					inline void add(const Track& track) {
						m_tracks.add(track);
					}

					/**
					 * @brief Appends @p track. Not idempotent. Order is mux order.
					 * @param track Track to take.
					 */
					inline void add(Track&& track) {
						m_tracks.add(std::move(track));
					}

					/**
					 * @brief Tube tracks. Index is mux slot.
					 * @return Track list.
					 */
					inline const class Tracks& Tracks() const noexcept {
						return m_tracks;
					}

					/**
					 * @brief Active intention flags.
					 * @return Mask. EnforceFeatures is set at construction.
					 */
					inline const class Policies& Policies() const noexcept {
						return m_policies;
					}

					/**
					 * @brief Adds @p policy, or clears it when passed as `~policy`.
					 * @param policy Flag to add, or the complement from @ref operator~(Policy).
					 *
					 * A value that only has defined bits is added. Other flags
					 * stay set. `~Policy::EnforceFeatures` clears only that
					 * flag. This does not replace the mask and does not take
					 * @ref Policies.
					 */
					inline void Policies(enum Policy policy) noexcept {
						constexpr auto defined = static_cast<std::uint32_t>(Policy::EnforceFeatures);
						const auto bits = static_cast<std::uint32_t>(policy);
						if ((bits & ~defined) == 0) {
							if (bits != 0)
								m_policies.Add(policy);
							return;
						}

						const auto clear = defined & ~bits;
						if (clear != 0)
							m_policies.Remove(static_cast<enum Policy>(clear));
					}

					/**
					 * @brief Resource policy carried by this plan.
					 * @return Best-effort policy, defaulting to @ref ResourceUsage::Full.
					 */
					constexpr ResourceUsage Resources() const noexcept {
						// Keep the installed Plan layout stable by encoding this internal
						// policy in reserved bits of its existing Policy mask.
						if (m_policies.Has(static_cast<Policy>(1u << 30)))
							return ResourceUsage::Low;
						if (m_policies.Has(static_cast<Policy>(1u << 29)))
							return ResourceUsage::Medium;
						return ResourceUsage::Full;
					}

					/**
					 * @brief Sets the resource policy before pipeline stages bind.
					 * @param usage Best-effort CPU and in-memory frame queue policy.
					 */
					inline void Resources(ResourceUsage usage) noexcept {
						const auto resourceBits = static_cast<Policy>((1u << 30) | (1u << 29));
						m_policies.Remove(resourceBits);
						switch (usage) {
							case ResourceUsage::Medium:
								m_policies.Add(static_cast<Policy>(1u << 29));
								break;
							case ResourceUsage::Low:
								m_policies.Add(static_cast<Policy>(1u << 30));
								break;
							case ResourceUsage::Full:
								break;
						}
					}

					/**
					 * @}
					 */

					/**
					 * @brief Whether this intention is well formed.
					 *
					 * Publishes Failed and preserves the first reason on validation failure.
					 * Construction validates locations and format separately, without tracks.
					 * Fails when: either location owner is empty; the constructor
					 * snapshot failed; no registered buffered muxer resolves; @ref Tracks is
					 * empty; @ref Track::In is negative; @ref Track::Type
					 * is @ref Type::Unknown; an attachment MIME is empty
					 * or is not an exact type/subtype, a type-star
					 * category, or star-star (all).
					 * Duplicate @ref Track::In is allowed. Encode knobs on a
					 * remux track (`Codec() == nullptr`) are ignored.
					 * With @ref Policy::EnforceFeatures, an encode whose
					 * destination codec has no implementation that keeps the
					 * source HDR10, HDR10+ or Dolby Vision features also fails.
					 * Other checks run either way.
					 *
					 * @return Empty value, or @ref PlanException.
					 */
					virtual CheckResult Check() const;

				private:
					/**
					 * @brief Origin reader owned by this Plan.
					 * @return Reader.
					 */
					StormByte::Buffer::IO::BufferedLocationReader& Reader() noexcept;

					/**
					 * @brief Origin reader owned by this Plan.
					 * @return Reader.
					 */
					const StormByte::Buffer::IO::BufferedLocationReader& Reader() const noexcept;

					/**
					 * @brief Consultation snapshot taken in the constructor.
					 * @return File snapshot (streams, attachments, metadata).
					 *
					 * Undefined if `!*this`. Not an octet source. Do not call
					 * File::Duration() on it while the tube reads.
					 */
					const StormByte::Multimedia::File& Snapshot() const noexcept;

					/**
					 * @brief Owned destination identity resolved during construction.
					 * @return Container copy; valid only after construction validation succeeds.
					 */
					inline const class Container& Container() const noexcept {
						return *m_container;
					}

					/**
					 * @brief Destination writer owned by this Plan.
					 * @return Writer.
					 */
					StormByte::Buffer::IO::BufferedLocationWriter& Writer() noexcept;

					/**
					 * @brief Destination writer owned by this Plan.
					 * @return Writer.
					 */
					const StormByte::Buffer::IO::BufferedLocationWriter& Writer() const noexcept;

					/**
					 * @brief Output path forwarded from the writer.
					 * @return Path.
					 */
					inline const StormByte::Safe::String& Path() const noexcept {
						return m_writer->Path();
					}

					/**
					 * @brief Allows the input stage to consume source data.
					 */
					friend class Demuxer;

					/**
					 * @brief Allows decoder snapshot consultation.
					 */
					friend class Decoder;

					/**
					 * @brief Allows encoder track consultation.
					 */
					friend class Encoder;

					/**
					 * @brief Allows remux snapshot consultation.
					 */
					friend class Remuxer;

					/**
					 * @brief Allows the output stage to consume destination data.
					 */
					friend class Muxer;

					/**
					 * @brief Allows filter matching against the source snapshot.
					 */
					friend class Filters;

					/**
					 * @brief Allows backend source restoration.
					 */
					friend class Backend::Pipeline::Demuxer;

					/**
					 * @brief Allows backend capacity calculation.
					 */
					friend class Backend::Pipeline::Ceiling;

					/**
					 * @brief Allows muxing with the resolved FFmpeg identity.
					 */
					friend class Backend::Pipeline::Detail::Muxer::FFmpeg::Container;

					/**
					 * @brief Allows stages to register state notifications.
					 */
					friend class Step;

					/**
					 * @brief Allows private route snapshot consultation.
					 */
					friend class Route;

					/**
					 * @brief Allows the public facade to publish failure and cancellation.
					 */
					friend class Transcoder;

					/**
					 * @brief Allows the backend coordinator to publish execution state.
					 */
					friend class Backend::Pipeline::Transcoder;

					/**
					 * @brief Applies a valid transition and preserves the first failure reason.
					 * @param desired Requested execution state.
					 * @param reason Optional reason used only for an accepted failure.
					 * @return Actual state after accepting or rejecting the transition.
					 */
					enum State State(enum State desired,
						StormByte::Safe::Optional<StormByte::Safe::String> reason = {}) const noexcept;

					/**
					 * @brief Registers a borrowed stage before checking the published state.
					 * @param step Stage kept alive through all notifications by its owner.
					 */
					void Observe(Step& step) noexcept;

					/**
					 * @brief Removes a borrowed stage during externally serialized teardown.
					 * @param step Stage whose owner has completed outstanding notifications.
					 */
					void Unobserve(Step& step) noexcept;

					/**
					 * @brief Holds or releases every stage at its next work unit.
					 * @param held True pauses the pipeline; false resumes it.
					 */
					void Hold(bool held) noexcept;

					/**
					 * @brief Blocks @p step while the plan is held.
					 * @param step Stage about to process; returns early once it is stopping.
					 */
					void WaitWhileHeld(const Step& step) const noexcept;

					/**
					 * @brief Records completion of a stage participating in this plan.
					 * @param step Participating stage whose execution has ended.
					 * @param success Whether the stage completed successfully.
					 */
					void Complete(Step& step, bool success) noexcept;

					/**
					 * @brief Provider-owned borrowed notification registry.
					 */
					class Observers;

					/**
					 * @brief Constructs locations, snapshot and a validated destination identity.
					 * @param reader Owned input location.
					 * @param writer Owned output location.
					 * @param container Explicit identity to copy; null guesses from the filename.
					 * @param duration Authoritative duration, or container estimate with scan fallback.
					 * @param progress Borrowed duration observer.
					 */
					Plan(StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
						StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
						const class Container* container,
						StormByte::Safe::Optional<std::int64_t> duration,
						const StormByte::Safe::Function<void(double)>* progress) noexcept;

					/**
					 * @brief Validates construction data and optionally all selected tracks.
					 * @param tracks Whether empty or invalid tracks are failures.
					 * @return Success or the preserved first plan failure.
					 */
					CheckResult Validate(bool tracks) const;

					StormByte::Safe::Shared<StormByte::Buffer::ReadTelemetry> m_input_telemetry;		///< Input counters retained independently of the reader.
					StormByte::Safe::Shared<StormByte::Buffer::WriteTelemetry> m_output_telemetry;		///< Output counters retained independently of the writer.
					StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> m_reader;	///< Owned origin octets.
					StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> m_writer;	///< Owned destination octets.
					StormByte::Safe::Unique<StormByte::Multimedia::File> m_snapshot;			///< Provider-owned constructor probe.
					const class Container* m_container = nullptr;			///< Borrowed destination identity owned by the registry.
					const char* m_format = nullptr;						///< Process-lifetime resolved FFmpeg muxer name.
					class Tracks m_tracks;								///< Tube tracks indexed by mux slot.
					mutable StormByte::Safe::Mutex m_state_mutex;		///< Synchronizes execution state and failure reason.
					mutable StormByte::Safe::Atomic<enum State> m_state{State::Created};	///< Published execution state.
					mutable StormByte::Safe::Optional<StormByte::Safe::String> m_error;	///< First supplied failure reason.
					StormByte::Safe::Unique<Observers> m_observers;	///< Provider-owned notification registry.

				protected:
					/**
					 * @brief Intention flags. Constructed with @ref Policy::EnforceFeatures.
					 *
					 * The public setter adds a flag, or clears it with `~flag`.
					 * Derived plans may also @ref StormByte::Bitmask::Remove.
					 */
					class Policies m_policies{Policy::EnforceFeatures};
			};

			/**
			 * @brief Transfers a shared intention without slicing its dynamic type.
			 * @param plan Provider-created intention owner.
			 * @param demuxer Destination stage.
			 * @return Destination stage.
			 */
			STORMBYTE_MULTIMEDIA_PUBLIC Demuxer& operator>>(StormByte::Safe::Shared<Plan> plan, Demuxer& demuxer) noexcept;
		}
	}
}

/**
 * @brief Registers the completed provider-owned plan.
 * @note Derived factories must construct the exact payload type in its provider module.
 */
/**
 * @brief Declare the allocation-free policy mask conditionally DLL-safe.
 * @note `class` selects the nested type; the same name is also a member function.
 * The mask stores only enum bits plus the Bitmask vtable. Default construction,
 * copy, move, assignment and destruction allocate nothing. Its inline operations
 * require a compatible provider ABI, with the multimedia provider loaded during use.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(class StormByte::Multimedia::Pipeline::Plan::Policies);
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Plan);

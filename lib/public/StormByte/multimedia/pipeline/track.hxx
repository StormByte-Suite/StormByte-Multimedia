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

#include <StormByte/multimedia/pipeline/config/base.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/clonable.hxx>
#include <StormByte/safe/vector.hxx>

/**
 * @namespace StormByte::Multimedia::Pipeline
 * @brief Demux / decode / filter / encode / mux types.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Pipeline {
	/**
	 * @class Track
	 * @brief One source stream that enters the tube.
	 *
	 * Intention only. Does not check that @ref In exists on the
	 * origin File, nor that @ref Type matches that stream.
	 * Consumers of @ref Plan validate.
	 *
	 * Mux slot is the index of this track in @ref Tracks, not a
	 * field. @ref Type is always stored. When a config is passed,
	 * @ref Type is copied from the leaf. A null @ref Config is an
	 * attachment (or no leaf). A non-null config with codec
	 * `nullptr` means Remux.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Track:
		public StormByte::Safe::Clonable<Track, StormByte::Safe::Shared<Track>> {
		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Track without a config leaf.
			 * @param in Origin stream index.
			 * @param type Media type (typically @ref StormByte::Multimedia::Type::Attachment).
			 */
			Track(int in, enum StormByte::Multimedia::Type type) noexcept;

			/**
			 * @brief Builds a track; clones @p config and copies its type.
			 * @param in Origin stream index.
			 * @param config Leaf config.
			 * @note Provider clone allocation failures propagate to the caller.
			 */
			Track(int in, const Config::Base& config);

			/**
			 * @brief Builds a track; moves @p config and copies its type.
			 * @param in Origin stream index.
			 * @param config Leaf config.
			 * @note Provider move allocation failures propagate to the caller.
			 */
			Track(int in, Config::Base&& config);

			/**
			 * @brief Copy constructor. Deep-copies @ref Config if present.
			 * @param other Source track.
			 */
			Track(const Track& other);

			/**
			 * @brief Move constructor.
			 * @param other Track to take.
			 */
			Track(Track&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			virtual ~Track() noexcept override;

			/**
			 * @brief Copy assignment. Deep-copies @ref Config if present.
			 * @param other Source track.
			 * @return *this.
			 */
			Track& operator=(const Track& other);

			/**
			 * @brief Move assignment.
			 * @param other Track to take.
			 * @return *this.
			 */
			Track& operator=(Track&& other) noexcept;

			/**
			 * @}
			 */

			/**
			 * @brief Deep copy.
			 * @return Owning pointer to a new @ref Track.
			 * @note Derived tracks must override in their provider module and
			 *       construct Shared<Track>::MakePointer<Derived> without slicing.
			 */
			PointerType Clone() const override;

			/**
			 * @brief Move into a new pointer.
			 * @return Owning pointer to the moved @ref Track.
			 * @note Derived tracks must override in their provider module and
			 *       retain the exact payload's creator-module destructor.
			 */
			PointerType Move() override;

			/**
			 * @name Intention
			 * @{
			 */

			/**
			 * @brief Media stamped at construction.
			 * @return Type from the no-config ctor or from the leaf.
			 */
			inline enum StormByte::Multimedia::Type Type() const noexcept {
				return m_type;
			}

			/**
			 * @brief Origin stream index.
			 * @return Index in the Plan source File.
			 */
			inline int In() const noexcept {
				return m_in;
			}

			/**
			 * @brief Track config leaf, if any.
			 * @return Leaf, or `nullptr` for attachments / no config.
			 */
			inline const Config::Base* Config() const noexcept {
				return m_config.get();
			}

			/**
			 * @}
			 */

		private:
			int m_in;												///< Origin stream index.

			enum StormByte::Multimedia::Type m_type;				///< Media stamped at construction.

			StormByte::Safe::Unique<Config::Base> m_config;			///< Provider-owned config leaf, or an empty owner.
	};
}

/**
 * @brief Registers the complete track with provider-owned config and lifecycle.
 * @note Base and Multimedia must remain loaded with compatible C++ ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Track);

/**
 * @namespace StormByte::Multimedia::Pipeline
 * @brief Demux / decode / filter / encode / mux types.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Pipeline {

	/**
	 * @class Tracks
	 * @brief Ordered tube tracks. Index is mux slot.
	 *
	 * Does not validate origin indexes or config. Consumers of
	 * @ref Plan do. `add` / `begin` / `end` / `size` / `empty`
	 * are lowercase on purpose: container surface.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Tracks {
		public:
			/**
			 * @brief Base-owned ordered track handles.
			 */
			using ContainerType = StormByte::Safe::Vector<StormByte::Safe::Shared<Track>>;

			/**
			 * @brief Count type.
			 */
			using size_type = ContainerType::size_type;

			/**
			 * @brief Read-only Safe iterator; dereference yields a shared track handle.
			 */
			using const_iterator = ContainerType::const_iterator;

			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Empty list.
			 */
			Tracks() noexcept;

			/**
			 * @brief Copy constructor. Clones every @ref Track.
			 * @param other Source list.
			 */
			Tracks(const Tracks& other);

			/**
			 * @brief Move constructor.
			 * @param other List to take.
			 */
			Tracks(Tracks&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			virtual ~Tracks() noexcept;

			/**
			 * @brief Copy assignment. Clones every @ref Track.
			 * @param other Source list.
			 * @return *this.
			 */
			Tracks& operator=(const Tracks& other);

			/**
			 * @brief Move assignment.
			 * @param other List to take.
			 * @return *this.
			 */
			Tracks& operator=(Tracks&& other) noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Container
			 * @{
			 */

			/**
			 * @brief Const begin.
			 * @return Iterator to the first shared track handle.
			 */
			inline const_iterator begin() const noexcept {
				return m_tracks.begin();
			}

			/**
			 * @brief Const end.
			 * @return Past-the-last iterator.
			 */
			inline const_iterator end() const noexcept {
				return m_tracks.end();
			}

			/**
			 * @brief Const begin.
			 * @return Iterator to the first element.
			 */
			inline const_iterator cbegin() const noexcept {
				return m_tracks.cbegin();
			}

			/**
			 * @brief Const end.
			 * @return Past-the-last iterator.
			 */
			inline const_iterator cend() const noexcept {
				return m_tracks.cend();
			}

			/**
			 * @brief Track at mux slot @p i.
			 * @param i Zero-based index.
			 * @return Borrowed track, valid while its owner remains in this list.
			 */
			const Track& operator[](size_type index) const;

			/**
			 * @brief Element count.
			 * @return Number of tracks.
			 */
			inline size_type size() const noexcept {
				return m_tracks.size();
			}

			/**
			 * @brief Whether there are no tracks.
			 * @return `true` if empty.
			 */
			inline bool empty() const noexcept {
				return m_tracks.empty();
			}

			/**
			 * @brief Appends a clone of @p track.
			 * @param track Track to copy.
			 */
			void add(const Track& track);

			/**
			 * @brief Appends @p track via @ref Track::Move.
			 * @param track Track to take.
			 */
			void add(Track&& track);

			/**
			 * @}
			 */

		private:
			ContainerType m_tracks;	///< Base-owned collection; all allocating operations run in Multimedia.
									///< @note Base and Multimedia must remain loaded with compatible C++ ABIs.
	};
}

/**
 * @brief Registers the completed collection with provider-owned storage and lifecycle.
 * @note Base and Multimedia must remain loaded with compatible C++ ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Tracks);

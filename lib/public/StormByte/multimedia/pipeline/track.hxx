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

#include <StormByte/iterable.hxx>
#include <StormByte/multimedia/pipeline/config/base.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/safe/clonable.hxx>
#include <StormByte/multimedia/visibility.h>

#include <memory>
#include <vector>

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
		public StormByte::Safe::Clonable<Track, StormByte::Safe::Unique<Track>> {
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
			 */
			Track(int in, const Config::Base& config) noexcept;

			/**
			 * @brief Builds a track; moves @p config and copies its type.
			 * @param in Origin stream index.
			 * @param config Leaf config.
			 */
			Track(int in, Config::Base&& config) noexcept;

			/**
			 * @brief Copy constructor. Deep-copies @ref Config if present.
			 * @param other Source track.
			 */
			Track(const Track& other);

			/**
			 * @brief Move constructor.
			 * @param other Track to take.
			 */
			Track(Track&& other) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			virtual ~Track() noexcept override = default;

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
			Track& operator=(Track&& other) noexcept = default;

			/**
			 * @}
			 */

			/**
			 * @brief Deep copy.
			 * @return Owning pointer to a new @ref Track.
			 */
			inline PointerType Clone() const override {
				return MakePointer<Track>(*this);
			}

			/**
			 * @brief Move into a new pointer.
			 * @return Owning pointer to the moved @ref Track.
			 */
			inline PointerType Move() override {
				return MakePointer<Track>(std::move(*this));
			}

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
			int m_in;										///< Origin stream index
			enum StormByte::Multimedia::Type m_type;		///< Media stamped at construction
			StormByte::Safe::Unique<Config::Base> m_config;		///< Leaf, or null
	};

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
	class STORMBYTE_MULTIMEDIA_PUBLIC Tracks:
		protected StormByte::Iterable<std::vector<StormByte::Safe::Unique<Track>>> {
		public:
			using size_type = StormByte::Iterable<std::vector<StormByte::Safe::Unique<Track>>>::size_type;			///< Count type
			using const_iterator = StormByte::Iterable<std::vector<StormByte::Safe::Unique<Track>>>::const_iterator;	///< Const iterator

			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Empty list.
			 */
			Tracks() noexcept = default;

			/**
			 * @brief Copy constructor. Clones every @ref Track.
			 * @param other Source list.
			 */
			Tracks(const Tracks& other);

			/**
			 * @brief Move constructor.
			 * @param other List to take.
			 */
			Tracks(Tracks&& other) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			virtual ~Tracks() noexcept = default;

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
			Tracks& operator=(Tracks&& other) noexcept = default;

			/**
			 * @}
			 */

			/**
			 * @name Container
			 * @{
			 */

			/**
			 * @brief Const begin.
			 * @return Iterator to the first element (`unique_ptr<Track>`).
			 */
			inline const_iterator begin() const noexcept {
				return Iterable::begin();
			}

			/**
			 * @brief Const end.
			 * @return Past-the-last iterator.
			 */
			inline const_iterator end() const noexcept {
				return Iterable::end();
			}

			/**
			 * @brief Const begin.
			 * @return Iterator to the first element.
			 */
			inline const_iterator cbegin() const noexcept {
				return Iterable::cbegin();
			}

			/**
			 * @brief Const end.
			 * @return Past-the-last iterator.
			 */
			inline const_iterator cend() const noexcept {
				return Iterable::cend();
			}

			/**
			 * @brief Track at mux slot @p i.
			 * @param i Zero-based index.
			 * @return Track.
			 */
			inline const Track& operator[](size_type i) const {
				return *Iterable::operator[](i);
			}

			/**
			 * @brief Element count.
			 * @return Number of tracks.
			 */
			inline size_type size() const noexcept {
				return Iterable::size();
			}

			/**
			 * @brief Whether there are no tracks.
			 * @return `true` if empty.
			 */
			inline bool empty() const noexcept {
				return Iterable::empty();
			}

			/**
			 * @brief Appends a clone of @p track.
			 * @param track Track to copy.
			 */
			inline void add(const Track& track) {
				Iterable::add(track.Clone());
			}

			/**
			 * @brief Appends @p track via @ref Track::Move.
			 * @param track Track to take.
			 */
			inline void add(Track&& track) {
				Iterable::add(track.Move());
			}

			/**
			 * @}
			 */
	};
}

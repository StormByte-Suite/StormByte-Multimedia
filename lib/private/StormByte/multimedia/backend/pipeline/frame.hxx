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

#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/string.hxx>

#include <string>
#include <utility>

extern "C" {
	struct AVFrame;
}

namespace StormByte::Multimedia::Pipeline {
	class Frame;
}

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @class Frame
	 * @brief Holder of one decoded access unit for the public Frame facade.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Frame {
		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Empty holder.
			 */
			Frame() noexcept = default;

			/**
			 * @brief Deep copy (cloned FFmpeg frame).
			 * @param other Source holder.
			 */
			Frame(const Frame& other) noexcept;

			/**
			 * @brief Move constructor.
			 * @param other Holder to take.
			 */
			Frame(Frame&& other) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			~Frame() noexcept = default;

			/**
			 * @brief Deep copy assignment (cloned FFmpeg frame).
			 * @param other Source holder.
			 * @return *this.
			 */
			Frame& operator=(const Frame& other) noexcept;

			/**
			 * @brief Move assignment.
			 * @param other Holder to take.
			 * @return *this.
			 */
			Frame& operator=(Frame&& other) noexcept = default;

			/**
			 * @}
			 */

			/**
			 * @name Handle
			 * @{
			 */

			/**
			 * @brief FFmpeg frame owned by this holder.
			 * @return Handle.
			 */
			inline const StormByte::Multimedia::FFmpeg::AVFrame& Handle() const noexcept {
				return m_handle;
			}

			/**
			 * @brief FFmpeg frame owned by this holder.
			 * @return Handle.
			 */
			inline StormByte::Multimedia::FFmpeg::AVFrame& Handle() noexcept {
				return m_handle;
			}

			/**
			 * @brief Replaces the owned FFmpeg frame.
			 * @param handle Frame to take.
			 */
			inline void Handle(StormByte::Multimedia::FFmpeg::AVFrame handle) noexcept {
				m_handle = std::move(handle);
			}

			/**
			 * @}
			 */

			/**
			 * @name Payload
			 * @{
			 */

			/**
			 * @brief Whether the public unit already materialised planes.
			 * @return true after Payload copied the primary buffer.
			 */
			inline bool PayloadReady() const noexcept {
				return m_payloadReady;
			}

			/**
			 * @brief Marks planes as materialised or stale.
			 * @param ready true after Payload copies the primary buffer.
			 */
			inline void PayloadReady(bool ready) noexcept {
				m_payloadReady = ready;
			}

			/**
			 * @}
			 */

			/**
			 * @brief Rebuilds Video or Audio on @p frame from the owned handle.
			 * @param frame Public unit that owns this holder.
			 */
			void BindProperties(StormByte::Multimedia::Pipeline::Frame& frame) noexcept;

			/**
			 * @brief Install @p raw, run Content extras, wrap properties.
			 * @param owner Public unit (Type() selects the Content leaf).
			 * @param raw Libav frame allocated by the caller. Ownership transfers.
			 *
			 * Content sees only raw pointers. Coupled extras that cannot
			 * be remapped are dropped; @ref Warning describes that. This
			 * reports required metadata failures through @ref ContentValid.
			 */
			void Put(StormByte::Multimedia::Pipeline::Frame& owner, ::AVFrame* raw) noexcept;

			/**
			 * @brief Why Content dropped extras on the last @ref Put.
			 * @return Empty when extras were kept or remapped.
			 */
			const StormByte::Safe::String& Warning() const noexcept;

			/**
			 * @brief LowLevel details from the most recent payload replacement.
			 * @return Empty when no replacement details were recorded.
			 */
			const StormByte::Safe::String& Diagnostic() const noexcept;

			/**
			 * @brief Reports whether required side data and properties survived replacement.
			 * @return False after side-data allocation or property binding failed.
			 */
			bool ContentValid() const noexcept;

		private:
			StormByte::Multimedia::FFmpeg::AVFrame m_handle;	///< FFmpeg frame
			bool m_payloadReady = false;								///< true after Payload materialised planes
			StormByte::Safe::String m_warning;									///< Last Content drop reason
			StormByte::Safe::String m_diagnostic;								///< Last Content replacement details
			bool m_contentValid = true;								///< Required metadata preservation status
	};
}

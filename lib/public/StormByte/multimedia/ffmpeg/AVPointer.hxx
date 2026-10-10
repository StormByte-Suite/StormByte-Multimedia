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

#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/type_traits.hxx>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Public Multimedia module.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::FFmpeg
		 * @brief Private RAII wrappers over libav*.
		 */
		namespace FFmpeg {
			/**
			 * @class AVPointer
			 * @brief Move-only RAII base for FFmpeg C pointers.
			 * @tparam AVType Underlying FFmpeg struct type.
			 *
			 * Derived classes must implement Free(). Get() is protected; each
			 * derived type re-exports it as private so only its friends (other
			 * wrappers) may touch the raw pointer.
			 * @note This base stores and transfers a raw pointer without allocating or
			 * releasing its resource in the empty destructor. Move assignment dispatches
			 * Free() virtually before adopting the source pointer. Concrete destructors
			 * must call their Free() implementation; borrowed resources must not be freed.
			 * Subclasses must keep heap-affecting operations in their provider module.
			 * Resource owners must outlive borrowed views, and Multimedia, Base, FFmpeg
			 * and subclass providers must remain loaded with compatible C++ and FFmpeg ABIs.
			 * Supported handle specializations are conditionally MaybeSafe, not universally
			 * Safe. Base Safe pointer factories provide the object heap and destruction
			 * route; they do not certify arbitrary subclasses or extend provider lifetimes.
			 */
			template<typename AVType>
			class STORMBYTE_MULTIMEDIA_PUBLIC AVPointer {
				public:
					/**
					 * @brief Default constructor (deleted).
					 *
					 * An empty wrapper is not useful; derived types adopt a pointer
					 * through the explicit constructor or a factory such as Open().
					 */
					constexpr AVPointer() noexcept = delete;

					/**
					 * @brief Copy constructor (deleted).
					 */
					constexpr AVPointer(const AVPointer&) noexcept = delete;

					/**
					 * @brief Move constructor.
					 * @param other Source wrapper.
					 */
					constexpr AVPointer(AVPointer&& other) noexcept
						: m_ptr(other.m_ptr) {
						other.m_ptr = nullptr;
					}

					/**
					 * @brief Destructor. Does not free; derived Free() owns that.
					 */
					virtual ~AVPointer() noexcept = default;

					/**
					 * @brief Copy assignment (deleted).
					 * @return *this.
					 */
					constexpr AVPointer& operator=(const AVPointer&) noexcept = delete;

					/**
					 * @brief Move assignment.
					 * @param other Source wrapper.
					 * @return *this.
					 */
					constexpr AVPointer& operator=(AVPointer&& other) noexcept {
						if (this != &other) {
							Free();
							m_ptr = other.m_ptr;
							other.m_ptr = nullptr;
						}
						return *this;
					}

				protected:
					std::decay_t<AVType>* m_ptr = nullptr;	///< FFmpeg pointer; concrete wrapper defines ownership.

					/**
					 * @brief Const view of the raw FFmpeg pointer.
					 * @return Pointer or nullptr.
					 *
					 * Not public. Derived wrappers expose typed work;
					 * friends of the derived class may `using` this as private.
					 */
					constexpr const std::decay_t<AVType>* Get() const noexcept {
						return m_ptr;
					}

					/**
					 * @brief Mutable view of the raw FFmpeg pointer.
					 * @return Pointer or nullptr.
					 */
					constexpr std::decay_t<AVType>* Get() noexcept {
						return m_ptr;
					}

					/**
					 * @brief Yields the raw pointer and leaves this wrapper empty.
					 * @return Previous pointer, or nullptr. Does not Free.
					 *
					 * Ownership moves to the caller. The wrapper destructor
					 * must not free a detached pointer. Not public; derived
					 * types `using` this as private for friends such as
					 * @c Filter::FFmpeg::Save.
					 */
					constexpr std::decay_t<AVType>* Detach() noexcept {
						auto* ptr = m_ptr;
						m_ptr = nullptr;
						return ptr;
					}

					/**
					 * @brief Adopts @p ptr.
					 * @param ptr Raw pointer.
					 */
					explicit constexpr AVPointer(std::decay_t<AVType>* ptr) noexcept
						: m_ptr(ptr) {}

					/**
					 * @brief Releases the underlying resource.
					 * @note Implementations must respect borrowing and release owned resources
					 * through their allocating provider. Concrete destructors must invoke this
					 * before the empty base destructor runs.
					 */
					virtual void Free() noexcept = 0;
			};
		}
	}
}

/**
 * @brief Conditional provider contracts for supported FFmpeg pointer bases.
 * @note The base type is complete even when its C handle is only forward declared.
 * Borrowing, virtual Free(), concrete destruction and provider ABI/lifetime
 * requirements remain the responsibility of each concrete wrapper or subclass.
 * Other handle types and arbitrary subclasses are not classified by these declarations.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVPointer<::AVAudioFifo>);

/**
 * @brief Conditional provider contract for the FFmpeg bitstream-filter pointer base.
 * @note Concrete wrappers must honor the ownership and provider requirements of AVPointer.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVPointer<::AVBSFContext>);

/**
 * @brief Conditional provider contract for the FFmpeg codec-context pointer base.
 * @note Concrete wrappers must honor the ownership and provider requirements of AVPointer.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVPointer<::AVCodecContext>);

/**
 * @brief Conditional provider contract for the FFmpeg codec-parameters pointer base.
 * @note Concrete wrappers must honor the ownership and provider requirements of AVPointer.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVPointer<::AVCodecParameters>);

/**
 * @brief Conditional provider contract for the FFmpeg dictionary pointer base.
 * @note Concrete wrappers must honor the ownership and provider requirements of AVPointer.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVPointer<::AVDictionary>);

/**
 * @brief Conditional provider contract for the FFmpeg filter-graph pointer base.
 * @note Concrete wrappers must honor the ownership and provider requirements of AVPointer.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVPointer<::AVFilterGraph>);

/**
 * @brief Conditional provider contract for the FFmpeg format-context pointer base.
 * @note Concrete wrappers must honor the ownership and provider requirements of AVPointer.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVPointer<::AVFormatContext>);

/**
 * @brief Conditional provider contract for the FFmpeg frame pointer base.
 * @note Concrete wrappers must honor the ownership and provider requirements of AVPointer.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVPointer<::AVFrame>);

/**
 * @brief Conditional provider contract for the FFmpeg packet pointer base.
 * @note Concrete wrappers must honor the ownership and provider requirements of AVPointer.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVPointer<::AVPacket>);

/**
 * @brief Conditional provider contract for the FFmpeg audio-resampler pointer base.
 * @note Concrete wrappers must honor the ownership and provider requirements of AVPointer.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVPointer<::SwrContext>);

/**
 * @brief Conditional provider contract for the FFmpeg image-scaler pointer base.
 * @note Concrete wrappers must honor the ownership and provider requirements of AVPointer.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVPointer<::SwsContext>);

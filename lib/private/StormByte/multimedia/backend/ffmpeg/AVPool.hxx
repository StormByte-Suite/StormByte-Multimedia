#pragma once

#include <StormByte/multimedia/visibility.h>

#include <array>
#include <cstddef>
#include <memory>

struct AVBufferPool;
struct AVCodecContext;
struct AVFrame;

/**
 * @namespace StormByte::Multimedia::Backend::FFmpeg
 * @brief FFmpeg memory owned by the Multimedia provider.
 */
namespace StormByte::Multimedia::Backend::FFmpeg {
	/**
	 * @brief Provider-owned software-video pools shared by compatible layouts.
	 * @note Separate live frames never share writable buffers. Native buffer
	 * references remain valid after the last owner closes a pool.
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE AVPool final {
		public:
			/**
			 * @brief Copy construction is not allowed.
			 * @param other Source pool.
			 */
			AVPool(const AVPool& other) = delete;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Source pool.
			 * @return This pool.
			 */
			AVPool& operator=(const AVPool& other) = delete;

			/**
			 * @brief Releases cached buffers, deferring live-buffer release to FFmpeg.
			 */
			~AVPool() noexcept;

			/**
			 * @brief Gets a shared owner for the decoder's aligned image layout.
			 * @param context Decoder supplying alignment requirements.
			 * @param frame Image dimensions and format.
			 * @return Provider-local owner, or empty on invalid layout/allocation failure.
			 * @note The synchronized registry holds weak references only. All participating
			 * decoders and callback providers must remain in a loaded Multimedia module.
			 */
			static std::shared_ptr<AVPool> For(::AVCodecContext& context, const ::AVFrame& frame) noexcept;

			/**
			 * @brief Acquires independent native buffers for an empty video frame.
			 * @param frame Image to fill with plane pointers and aligned strides.
			 * @return Zero on success, or a negative FFmpeg error code.
			 */
			int Fill(::AVFrame& frame) noexcept;

			/**
			 * @brief Installs shared software allocation before opening a video decoder.
			 * @param context Fresh codec context with unused opaque state.
			 * @return Zero on success, or a negative FFmpeg error code.
			 * @note Audio, non-DR1 codecs and custom allocators are left unchanged.
			 */
			static int Attach(::AVCodecContext& context) noexcept;

			/**
			 * @brief Closes and nulls a codec context, then releases its pool owner.
			 * @param context Codec pointer to release after its users have stopped.
			 */
			static void Release(::AVCodecContext*& context) noexcept;

		private:
			/**
			 * @brief Format, aligned width/height, four strides and allocation alignment.
			 */
			using Key = std::array<int, 8>;

			/**
			 * @brief Creates the native per-plane pools for a validated layout.
			 * @param key Layout identity.
			 * @param sizes Per-plane sizes including decoder overread padding.
			 */
			AVPool(Key key, const std::array<std::size_t, 4>& sizes) noexcept;

			/**
			 * @brief Thread-safe FFmpeg allocation callback.
			 * @param context Decoder or frame-thread context.
			 * @param frame Empty output frame.
			 * @param flags Flags forwarded to hardware/default allocation.
			 * @return Zero on success, or a negative FFmpeg error code.
			 */
			static int GetBuffer(::AVCodecContext* context, ::AVFrame* frame, int flags) noexcept;

			Key m_key;                                  ///< Layout identity and strides.
			std::array<::AVBufferPool*, 4> m_pools{};    ///< Native per-plane owners.
			bool m_valid = true;                       ///< All required pools opened.
	};
}
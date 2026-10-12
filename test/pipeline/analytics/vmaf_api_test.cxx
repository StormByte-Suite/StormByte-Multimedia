#include "../helpers.hxx"

#include <array>
#include <cmath>
#include <cstring>

extern "C" {
	#include <libvmaf/libvmaf.h>
	#include <libvmaf/model.h>
	#include <libvmaf/picture.h>
}

namespace {
	class Context final {
		public:
			Context() = default;

			Context(const Context& other) = delete;

			Context(Context&& other) noexcept = delete;

			~Context() noexcept {
				if (m_context)
					vmaf_close(m_context);
				if (m_model)
					vmaf_model_destroy(m_model);
			}

			Context& operator=(const Context& other) = delete;

			Context& operator=(Context&& other) noexcept = delete;

			VmafContext* m_context = nullptr;
			VmafModel* m_model = nullptr;
	};

	class Picture final {
		public:
			Picture() = default;

			Picture(const Picture& other) = delete;

			Picture(Picture&& other) noexcept = delete;

			~Picture() noexcept {
				if (m_picture.ref)
					vmaf_picture_unref(&m_picture);
			}

			Picture& operator=(const Picture& other) = delete;

			Picture& operator=(Picture&& other) noexcept = delete;

			VmafPicture m_picture{};
	};
}

// -------------------
// Picture
// -------------------

int test_vmaf_picture_api_contract() {
	std::cout << "[VMAF] version=" << vmaf_version() << " picture_bytes=" << sizeof(VmafPicture) << std::endl;
	for (const unsigned depth : {8u, 10u}) {
		Context owner;
		VmafConfiguration configuration{};
		configuration.n_threads = 2;
		configuration.n_subsample = 1;
		configuration.log_level = VMAF_LOG_LEVEL_WARNING;
		VmafModelConfig modelConfiguration{};
		modelConfiguration.name = "vmaf";
		TEST_REQUIRE(vmaf_init(&owner.m_context, configuration) == 0);
		TEST_REQUIRE(vmaf_model_load(&owner.m_model, &modelConfiguration, "vmaf_v0.6.1") == 0);
		TEST_REQUIRE(vmaf_use_features_from_model(owner.m_context, owner.m_model) == 0);
		for (unsigned index = 0; index < 3; ++index) {
			Picture reference;
			Picture distorted;
			for (auto* picture : {&reference.m_picture, &distorted.m_picture}) {
				if (index == 0)
					TEST_REQUIRE(vmaf_picture_alloc(picture, VMAF_PIX_FMT_YUV420P, depth, 320, 192) == 0);
				else
					TEST_REQUIRE(vmaf_fetch_preallocated_picture(owner.m_context, picture) == 0);
				TEST_REQUIRE(picture->pix_fmt == VMAF_PIX_FMT_YUV420P);
				TEST_REQUIRE(picture->bpc == depth);
				TEST_REQUIRE(picture->w[0] == 320 && picture->h[0] == 192);
				for (unsigned plane = 0; plane < 3; ++plane) {
					TEST_REQUIRE(picture->data[plane] && picture->stride[plane] > 0);
					std::memset(picture->data[plane], 0, picture->stride[plane] * picture->h[plane]);
				}
			}
			TEST_REQUIRE(vmaf_read_pictures(owner.m_context, &reference.m_picture, &distorted.m_picture, index) == 0);
		}
		TEST_REQUIRE(vmaf_read_pictures(owner.m_context, nullptr, nullptr, 0) == 0);
		double score = 0;
		TEST_REQUIRE(vmaf_score_pooled(owner.m_context, owner.m_model, VMAF_POOL_METHOD_MEAN, &score, 0, 2) == 0);
		TEST_REQUIRE(std::isfinite(score) && score >= 0 && score <= 100);
	}
	return 0;
}

int main(int argc, char** argv) {
	const std::array tests{
		// -------------------
		// Picture
		// -------------------
		TestEntry{"test_vmaf_picture_api_contract", test_vmaf_picture_api_contract},
	};
	return RunSelectedTest(argc, argv, tests);
}
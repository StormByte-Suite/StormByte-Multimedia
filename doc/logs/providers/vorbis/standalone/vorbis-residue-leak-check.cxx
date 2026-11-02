#include <vorbis/vorbisenc.h>

#include <cstdio>

int main(int argc, char **) {
	const int rates[] = {8000, 16000, 22050, 32000, 44100, 48000};
	const float qualities[] = {0.0f, 0.4f, 0.9f};
	unsigned cases = 0;
	if (argc == 1) {
		vorbis_info info;
		vorbis_info_init(&info);
		const int result = vorbis_encode_init_vbr(&info, 6, 48000, 0.4f);
		vorbis_info_clear(&info);
		if (result != 0) {
			std::fprintf(stderr, "Surround setup failed: %d\n", result);
			return 1;
		}
		std::puts("Surround setup and clear completed; LSan checks allocation ownership.");
		return 0;
	}
	for (const int rate : rates) {
		for (int channels = 1; channels <= 8; ++channels) {
			for (const float quality : qualities) {
				vorbis_info info;
				vorbis_info_init(&info);
				const int result = vorbis_encode_init_vbr(&info, channels, rate, quality);
				vorbis_info_clear(&info);
				if (result != 0) {
					std::fprintf(stderr, "Setup failed: rate=%d channels=%d quality=%f result=%d\n",
						rate, channels, quality, result);
					return 1;
				}
				++cases;
			}
		}
	}
	std::printf("Vorbis setup/clear matrix completed: %u configurations.\n", cases);
}

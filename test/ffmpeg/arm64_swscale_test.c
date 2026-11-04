#include <stdint.h>
#include <stdio.h>

extern void ff_yuv2planeX_10_neon(const int16_t *filter, int filter_size,
                                 const int16_t **source, uint16_t *destination,
                                 int width, int big_endian, int depth);

static int16_t rows[2][48];
static uint16_t output[64];
static const int16_t patterns[] = {-16000, -1000, -1, 0, 1, 1000, 16000, 32760};
static const int16_t filters[][2] = {
	{4096, 0}, {8191, -4095}, {-4095, 8191}, {2048, 2048}
};
static const int depths[] = {9, 10, 12, 14};

static uint16_t reference_pixel(int x, const int16_t *filter, int depth, int big_endian) {
	const int shift = 27 - depth;
	int64_t value = ((int64_t)1 << (shift - 1))
		+ (int64_t)rows[0][x] * filter[0] + (int64_t)rows[1][x] * filter[1];
	value >>= shift;
	const int maximum = (1 << depth) - 1;
	if (value < 0)
		value = 0;
	if (value > maximum)
		value = maximum;
	const unsigned result = (unsigned)value;
	return (uint16_t)(big_endian ? ((result & 255U) << 8) | (result >> 8) : result);
}

int main(void) {
	const int16_t *source[] = {rows[0], rows[1]};
	unsigned cases = 0;
	for (int pattern = 0; pattern < 8; ++pattern) {
		for (int x = 0; x < 48; ++x) {
			rows[0][x] = patterns[(x + pattern) % 8];
			rows[1][x] = patterns[(x * 3 + pattern + 1) % 8];
		}
		for (int d = 0; d < 4; ++d) {
			for (int big_endian = 0; big_endian < 2; ++big_endian) {
				for (int f = 0; f < 4; ++f) {
					for (int width = 1; width <= 39; ++width) {
						for (int x = 0; x < 64; ++x)
							output[x] = 0xdead;
						ff_yuv2planeX_10_neon(filters[f], 2, source, output,
							width, big_endian, depths[d]);
						++cases;
						for (int x = 0; x < 64; ++x) {
							const uint16_t expected = x < width
								? reference_pixel(x, filters[f], depths[d], big_endian)
								: 0xdead;
							if (output[x] != expected) {
								fprintf(stderr,
									"swscale mismatch: pattern=%d depth=%d endian=%d "
									"filter=%d width=%d pixel=%d expected=%04x got=%04x\n",
									pattern, depths[d], big_endian, f, width, x,
									(unsigned)expected, (unsigned)output[x]);
								return 1;
							}
						}
					}
				}
			}
		}
	}
	printf("arm64 swscale: %u cases passed\n", cases);
	return 0;
}

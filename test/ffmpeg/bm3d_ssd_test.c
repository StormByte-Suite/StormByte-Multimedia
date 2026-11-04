#include <stdint.h>
#include <stdio.h>

typedef struct BM3DContext {
	int block_size;
} BM3DContext;

typedef struct PosCode {
	int x, y;
} PosCode;

#include "bm3d_ssd.h"

static uint16_t pixels[80 * 80];

static double reference(const uint8_t *source, int stride, int depth,
	int size, PosCode position, int row, int column) {
	double distance = 0;
	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			const uint8_t *a = source + (row + y) * stride;
			const uint8_t *b = source + (position.y + y) * stride;
			const double difference = depth == 8 ? a[column + x] - b[position.x + x]
				: ((const uint16_t *)a)[column + x] - ((const uint16_t *)b)[position.x + x];
			distance += difference * difference;
		}
	}
	return distance;
}

int main(void) {
	static const int sizes[] = {8, 9, 16, 31, 64};
	static const int depths[] = {8, 9, 10, 12, 14, 16};
	unsigned cases = 0;
	for (unsigned d = 0; d < sizeof(depths) / sizeof(*depths); ++d) {
		const int depth = depths[d];
		const unsigned maximum = (1u << depth) - 1;
		for (int pattern = 0; pattern < 4; ++pattern) {
			uint32_t random = 17;
			for (unsigned i = 0; i < sizeof(pixels) / sizeof(*pixels); ++i) {
				random = random * 1664525u + 1013904223u;
				const unsigned value = pattern == 0 ? 0 : pattern == 1 ? maximum
					: pattern == 2 ? (i % 80 < 40 ? 0 : maximum) : random & maximum;
				if (depth == 8)
					((uint8_t *)pixels)[i] = value;
				else
					pixels[i] = value;
			}
			for (int reversed = 0; reversed < 2; ++reversed) {
				const int row_bytes = 80 * (depth == 8 ? 1 : 2);
				const int stride = reversed ? -row_bytes : row_bytes;
				const uint8_t *source = (const uint8_t *)pixels + (reversed ? 79 * row_bytes : 0);
				for (unsigned b = 0; b < sizeof(sizes) / sizeof(*sizes); ++b) {
					BM3DContext context = {sizes[b]};
					for (int offset = 0; offset < 4; ++offset) {
						PosCode position = {offset + 2, offset + 1};
						const double expected = reference(source, stride, depth, context.block_size, position, 1, 0);
						const double limits[] = {0, 1, expected / 2, expected - 1, expected, expected + 1, 17592186044416.0};
						for (unsigned l = 0; l < sizeof(limits) / sizeof(*limits); ++l) {
							const double actual = depth == 8
								? do_block_ssd(&context, &position, source, stride, 1, 0, limits[l])
								: do_block_ssd16(&context, &position, source, stride, 1, 0, limits[l]);
							if ((expected <= limits[l] && actual != expected) ||
								(expected > limits[l] && !(actual > limits[l]))) {
								fprintf(stderr, "BM3D SSD mismatch: depth=%d block=%d pattern=%d reversed=%d limit=%.0f expected=%.0f actual=%.0f\n",
									depth, context.block_size, pattern, reversed, limits[l], expected, actual);
								return 1;
							}
							++cases;
						}
					}
				}
			}
		}
	}
	printf("BM3D SSD: %u exact-distance and rejection cases passed\n", cases);
	return 0;
}

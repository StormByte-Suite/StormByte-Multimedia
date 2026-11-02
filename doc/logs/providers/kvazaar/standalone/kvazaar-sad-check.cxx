#define INLINE inline
#include "strategies/sse41/reg_sad_pow2_widths-sse41.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

int main() {
  unsigned char a[2048], b[2048];
  for (int i = 0; i < 2048; ++i) {
    a[i] = static_cast<unsigned char>(i * 47 + i / 7);
    b[i] = static_cast<unsigned char>(i * 23 + i / 11);
  }
  unsigned trials = 0;
  for (int offsetA = 0; offsetA < 16; ++offsetA) {
    for (int offsetB = 0; offsetB < 16; ++offsetB) {
      for (int height = 1; height <= 31; ++height) {
        for (int width : {4, 8}) {
          unsigned regular = 0, vertical = 0;
          for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
              regular += std::abs(int(a[offsetA + y * 37 + x]) -
                                  int(b[offsetB + y * 39 + x]));
              vertical += std::abs(int(a[offsetA + y * 37 + x]) -
                                   int(b[offsetB + x]));
            }
          }
          auto reg = width == 4 ? reg_sad_w4 : reg_sad_w8;
          auto ver = width == 4 ? ver_sad_w4 : ver_sad_w8;
          assert(reg(a + offsetA, b + offsetB, height, 37, 39) == regular);
          assert(ver(a + offsetA, b + offsetB, height, 37) == vertical);
          ++trials;
        }
      }
    }
  }
  std::printf("Unaligned SAD matches scalar reference: %u trials\n", trials);
}

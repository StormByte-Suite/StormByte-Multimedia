#include "common_fix.h"
#include "scale.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>

int main() {
  const FIXP_DBL values[] = {std::numeric_limits<FIXP_DBL>::min(),
                             std::numeric_limits<FIXP_DBL>::min() + 1,
                             -3114762, -3, -2, -1, 0, 1, 2, 3,
                             std::numeric_limits<FIXP_DBL>::max()};
  std::uint32_t state = 31;
  for (int trial = 0; trial < 10011; ++trial) {
    state = state * 1664525u + 1013904223u;
    FIXP_DBL value = trial < 11 ? values[trial] : static_cast<FIXP_DBL>(state);
    for (int shift = -31; shift <= 31; ++shift) {
      FIXP_DBL expected = shift >= 0
          ? static_cast<FIXP_DBL>(static_cast<ULONG>(value) << shift)
          : value >> -shift;
      assert(scaleValue(value, shift) == expected);
      FIXP_DBL inPlace = value;
      scaleValueInPlace(&inPlace, shift);
      assert(inPlace == expected);

      INT64 saturated;
      if (shift >= 0) {
        saturated = static_cast<INT64>(value) * (INT64{1} << shift);
      } else {
        std::uint32_t magnitude = static_cast<std::uint32_t>(
            value ^ (value >> 31));
        saturated = magnitude < (std::uint32_t{1} << -shift)
            ? 0 : value >> -shift;
      }
      saturated = std::clamp(saturated,
          static_cast<INT64>(std::numeric_limits<FIXP_DBL>::min()) + 1,
          static_cast<INT64>(std::numeric_limits<FIXP_DBL>::max()));
      assert(scaleValueSaturate(value, shift) == saturated);
    }
    for (int bits : {8, 16, 24, 32}) {
      INT64 low = -(INT64{1} << (bits - 1));
      INT64 high = (INT64{1} << (bits - 1)) - 1;
      for (int shift = 0; shift <= 31; ++shift) {
        INT64 product = static_cast<INT64>(value) * (INT64{1} << shift);
        if (SATURATE_LEFT_SHIFT(value, shift, bits) !=
            std::clamp(product, low, high)) {
          std::fprintf(stderr, "value=%d shift=%d bits=%d result=%d expected=%lld\n",
              value, shift, bits, SATURATE_LEFT_SHIFT(value, shift, bits),
              static_cast<long long>(std::clamp(product, low, high)));
          return 1;
        }
        assert(SATURATE_LEFT_SHIFT_ALT(value, shift, bits) ==
               std::clamp(product, low + 1, high));
      }
    }
  }
  std::puts("Scalar/in-place/saturating scaling matches widened reference: 10011 trials");
}

#include "common_fix.h"
#include "fixmul.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>

int main() {
  static_assert(sizeof(LONG) == 4 && sizeof(ULONG) == 4 && sizeof(SHORT) == 2);
  const LONG values[] = {std::numeric_limits<LONG>::min(),
                         std::numeric_limits<LONG>::min() + 1, -65537, -1,
                         0, 1, 65537, std::numeric_limits<LONG>::max()};
  const SHORT shorts[] = {-32768, -32767, -1, 0, 1, 32767};
  auto doubled = [](INT64 value) {
    return static_cast<ULONG>(static_cast<std::uint64_t>(value) * 2);
  };
  std::uint32_t state = 17;
  for (int trial = 0; trial < 100008; ++trial) {
    state = state * 1664525u + 1013904223u;
    LONG a = trial < 8 ? values[trial] : static_cast<LONG>(state);
    for (LONG b : values) {
      ULONG expected = doubled((static_cast<INT64>(a) * b) >> 32);
      assert(static_cast<ULONG>(fixmul_DD(a, b)) == expected);
      assert(static_cast<ULONG>(fixmulBitExact_DD(a, b)) == expected);
    }
    for (SHORT b : shorts) {
      ULONG expected = doubled((static_cast<INT64>(a) * b) >> 16);
      assert(static_cast<ULONG>(fixmul_DS(a, b)) == expected);
      assert(static_cast<ULONG>(fixmul_SD(b, a)) == expected);
      assert(static_cast<ULONG>(fixmulBitExact_DS(a, b)) == expected);
      assert(static_cast<ULONG>(fixmulBitExact_SD(b, a)) == expected);
    }
  }
  for (SHORT a : shorts) {
    for (SHORT b : shorts) {
      assert(static_cast<ULONG>(fixmul_SS(a, b)) ==
             doubled(static_cast<INT64>(a) * b));
    }
  }
  std::puts("Fixed-point multiply variants match widened reference: 100008 trials");
}

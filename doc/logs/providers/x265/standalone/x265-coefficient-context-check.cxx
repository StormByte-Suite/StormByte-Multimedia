#include "cpu.h"
#include "quant.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

int main() {
    uint64_t state = 0x123456789abcdef0ULL;
    unsigned cases = 0;
    for (unsigned trial = 0; trial < 1000; ++trial) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        for (const unsigned size : {2U, 4U, 8U}) {
            for (unsigned y = 0; y < size; ++y) {
                for (unsigned x = 0; x < size; ++x) {
                    for (unsigned pos = 0; pos < size * size; ++pos) {
                        const unsigned right = x + 1 < size && pos + 1 < 64
                            ? (state >> (pos + 1)) & 1 : 0;
                        const unsigned lower = y + 1 < size && pos + size < 64
                            ? (state >> (pos + size)) & 1 : 0;
                        assert(X265_NS::Quant::calcPatternSigCtx(state, x, y, pos, size)
                               == right + 2 * lower);
                        assert(X265_NS::Quant::getSigCoeffGroupCtxInc(state, x, y, pos, size)
                               == (right | lower));
                        ++cases;
                    }
                }
            }
        }
    }
    std::printf("Coefficient contexts: %u comparisons passed, including group 63.\n", cases);
}

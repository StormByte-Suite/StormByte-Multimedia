#include "wels_common_defs.h"
#include "golomb_common.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

int main() {
    uint32_t random = 0x92e5b341;
    for (unsigned trial = 0; trial < 1000; ++trial) {
        std::array<uint8_t, 128> actual{};
        std::array<uint8_t, 128> expected{};
        WelsCommon::SBitStringAux writer{};
        WelsCommon::InitBits(&writer, actual.data(), actual.size());
        unsigned bits = 0;
        WelsCommon::BsFlush(&writer);
        assert(writer.pCurBuf == actual.data());
        for (unsigned part = 0; part < 16; ++part) {
            random = random * 1664525u + 1013904223u;
            const unsigned length = (random >> 27) + 1;
            random = random * 1664525u + 1013904223u;
            const uint32_t value = length == 32 ? random : random & ((1u << length) - 1);
            WelsCommon::BsWriteBits(&writer, length, value);
            for (unsigned bit = length; bit > 0; --bit, ++bits)
                expected[bits / 8] |= ((value >> (bit - 1)) & 1u) << (7 - bits % 8);
        }
        WelsCommon::BsFlush(&writer);
        assert(writer.pCurBuf == actual.data() + (bits + 7) / 8);
        assert(actual == expected);
        WelsCommon::BsFlush(&writer);
        assert(actual == expected);
    }
    std::puts("OpenH264: 1000 bitstreams and empty/repeated flushes match reference");
}

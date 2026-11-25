#ifndef TESTS_LIVEPHOTODECODE_LPMETAPARSERTEST_H
#define TESTS_LIVEPHOTODECODE_LPMETAPARSERTEST_H

#include <cstdint>
#include <cstring>
#include <vector>

#include "LivePhotoDecode/LpMetaParser.h"
#include "cli/TestAssert.h"

namespace {

// Encode a float to its big-endian IEEE754 byte representation.
std::vector<uint8_t> EncodeBigEndian(float v)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    std::vector<uint8_t> b(4);
    b[0] = static_cast<uint8_t>((bits >> 24) & 0xFF);
    b[1] = static_cast<uint8_t>((bits >> 16) & 0xFF);
    b[2] = static_cast<uint8_t>((bits >> 8) & 0xFF);
    b[3] = static_cast<uint8_t>(bits & 0xFF);
    return b;
}

} // namespace

inline void LpMetaParserUnitTest()
{
    // ParseBigEndianFloats: a single big-endian 1.0f (0x3F800000) must be
    // present in the scan output.
    {
        const std::vector<uint8_t> data = {0x3F, 0x80, 0x00, 0x00};
        const std::vector<float> floats = ParseBigEndianFloats(data);
        bool found = false;
        for (const float f : floats) {
            if (f == 1.0F) {
                found = true;
                break;
            }
        }
        CU_CHECK(found);
    }

    // BuildFrameMeta: anchor exposure=1.0 and iso=250.0 via a packet whose data
    // is the big-endian concatenation of those two floats. pts 90000 / den 90000 -> 1.0s.
    {
        const std::vector<uint8_t> expBytes = EncodeBigEndian(1.0F);
        const std::vector<uint8_t> isoBytes = EncodeBigEndian(250.0F);
        std::vector<uint8_t> data = expBytes;
        data.insert(data.end(), isoBytes.begin(), isoBytes.end());

        MettPacket pkt;
        pkt.pts = 90000;
        pkt.data = std::move(data);

        const std::vector<FrameMeta> metas = BuildFrameMeta({pkt}, 90000, 1.0, 250.0);

        CU_CHECK_EQ(static_cast<int>(metas.size()), 1);
        if (metas.size() == 1) {
            CU_CHECK_EQ(metas[0].frameIndex, 1);
            CU_CHECK(metas[0].ptsSeconds > 0.999 && metas[0].ptsSeconds < 1.001);
            CU_CHECK(metas[0].exposureTime > 0.999 && metas[0].exposureTime < 1.001);
            CU_CHECK(metas[0].iso > 249.0 && metas[0].iso < 251.0);
        }
    }

    // BuildFrameMeta: cover truth not present in the sample -> fields stay -1.
    {
        const std::vector<uint8_t> data = EncodeBigEndian(0.25F); // unrelated value
        MettPacket pkt;
        pkt.pts = 0;
        pkt.data = data;

        const std::vector<FrameMeta> metas = BuildFrameMeta({pkt}, 90000, 1.0, 250.0);

        CU_CHECK_EQ(static_cast<int>(metas.size()), 1);
        if (metas.size() == 1) {
            CU_CHECK(metas[0].exposureTime < 0.0);
            CU_CHECK(metas[0].iso < 0.0);
        }
    }
}

#endif // TESTS_LIVEPHOTODECODE_LPMETAPARSERTEST_H

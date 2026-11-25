#ifndef TESTS_LIVEPHOTODECODE_LPMETAPARSERTEST_H
#define TESTS_LIVEPHOTODECODE_LPMETAPARSERTEST_H

#include <cstdint>
#include <cstring>
#include <vector>

#include "LivePhotoDecode/LpMetaParser.h"
#include "cli/TestAssert.h"

namespace {

// Build a synthetic mett sample with the confirmed field layout:
//   +42 uint16 ISO, +60/+64/+68 float.
std::vector<uint8_t> MakeSample(uint16_t iso, float expA, float expB, float expC)
{
    std::vector<uint8_t> data(72, 0);

    data[42] = static_cast<uint8_t>((iso >> 8) & 0xFF);
    data[43] = static_cast<uint8_t>(iso & 0xFF);

    auto putFloat = [&](size_t off, float v) {
        uint32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        data[off] = static_cast<uint8_t>((bits >> 24) & 0xFF);
        data[off + 1] = static_cast<uint8_t>((bits >> 16) & 0xFF);
        data[off + 2] = static_cast<uint8_t>((bits >> 8) & 0xFF);
        data[off + 3] = static_cast<uint8_t>(bits & 0xFF);
    };

    putFloat(60, expA);
    putFloat(64, expB);
    putFloat(68, expC);
    return data;
}

} // namespace

inline void LpMetaParserUnitTest()
{
    // BuildFrameMeta: confirm ISO (uint16 at +42) and floats at +60/+64/+68.
    {
        MettPacket pkt;
        pkt.pts = 90000;
        pkt.data = MakeSample(1912, 38.2460938F, 39.76416F, 39.9128151F);

        const std::vector<FrameMeta> metas = BuildFrameMeta({pkt}, 90000);

        CU_CHECK_EQ(static_cast<int>(metas.size()), 1);
        if (metas.size() == 1) {
            CU_CHECK_EQ(metas[0].frameIndex, 1);
            CU_CHECK(metas[0].ptsSeconds > 0.999 && metas[0].ptsSeconds < 1.001);
            CU_CHECK_EQ(metas[0].iso, 1912);
            CU_CHECK(metas[0].shutterApexA > 38.24 && metas[0].shutterApexA < 38.25);
            CU_CHECK(metas[0].shutterApexB > 39.76 && metas[0].shutterApexB < 39.77);
            CU_CHECK(metas[0].shutterApexC > 39.91 && metas[0].shutterApexC < 39.92);
        }
    }

    // BuildFrameMeta: a short sample (missing fields) keeps -1 / -1 sentinels.
    {
        MettPacket pkt;
        pkt.pts = 0;
        pkt.data = {0x00, 0x0F, 0xA0, 0x0A}; // too short, no fields

        const std::vector<FrameMeta> metas = BuildFrameMeta({pkt}, 90000);

        CU_CHECK_EQ(static_cast<int>(metas.size()), 1);
        if (metas.size() == 1) {
            CU_CHECK_EQ(metas[0].iso, -1);
            CU_CHECK(metas[0].shutterApexA < 0.0);
            CU_CHECK(metas[0].shutterApexB < 0.0);
            CU_CHECK(metas[0].shutterApexC < 0.0);
        }
    }

    // BuildFrameMeta: ISO jump 382 -> 1912 across two samples is preserved verbatim.
    {
        MettPacket p1;
        p1.pts = 0;
        p1.data = MakeSample(382, 7.649414F, 7.95283175F, 7.98256254F);

        MettPacket p2;
        p2.pts = 3001;
        p2.data = MakeSample(1912, 38.2460938F, 39.76416F, 39.9128151F);

        const std::vector<FrameMeta> metas = BuildFrameMeta({p1, p2}, 90000);

        CU_CHECK_EQ(static_cast<int>(metas.size()), 2);
        if (metas.size() == 2) {
            CU_CHECK_EQ(metas[0].iso, 382);
            CU_CHECK_EQ(metas[1].iso, 1912);
            CU_CHECK(!metas[0].isHeader);
            CU_CHECK(!metas[1].isHeader);
        }
    }

    // BuildFrameMeta: the "SAT" header sample is flagged and its fields stay -1.
    {
        MettPacket pkt;
        pkt.pts = 0;
        pkt.data = std::vector<uint8_t>(72, 0);
        pkt.data[40] = 'S';
        pkt.data[41] = 'A';
        pkt.data[42] = 'T';

        const std::vector<FrameMeta> metas = BuildFrameMeta({pkt}, 90000);

        CU_CHECK_EQ(static_cast<int>(metas.size()), 1);
        if (metas.size() == 1) {
            CU_CHECK(metas[0].isHeader);
            CU_CHECK_EQ(metas[0].iso, -1);
            CU_CHECK(metas[0].shutterApexA < 0.0);
        }
    }
}

#endif // TESTS_LIVEPHOTODECODE_LPMETAPARSERTEST_H

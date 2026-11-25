#ifndef TESTS_LIVEPHOTODECODE_LPMETAPARSERTEST_H
#define TESTS_LIVEPHOTODECODE_LPMETAPARSERTEST_H

#include <cstdint>
#include <cstring>
#include <vector>

#include "LivePhotoDecode/LpMetaParser.h"
#include "cli/TestAssert.h"

namespace {

// Build a synthetic mett sample with the confirmed field layout:
//   +36 uint32 nanosecond timestamp, +42 uint16 ISO, +60/+64/+68 float gains.
std::vector<uint8_t> MakeSample(uint32_t timestampNs, uint16_t iso, float gainA, float gainB, float gainC)
{
    std::vector<uint8_t> data(72, 0);

    data[36] = static_cast<uint8_t>((timestampNs >> 24) & 0xFF);
    data[37] = static_cast<uint8_t>((timestampNs >> 16) & 0xFF);
    data[38] = static_cast<uint8_t>((timestampNs >> 8) & 0xFF);
    data[39] = static_cast<uint8_t>(timestampNs & 0xFF);

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

    putFloat(60, gainA);
    putFloat(64, gainB);
    putFloat(68, gainC);
    return data;
}

} // namespace

inline void LpMetaParserUnitTest()
{
    // BuildFrameMeta: confirm timestamp, ISO, and gain fields.
    {
        MettPacket pkt;
        pkt.pts = 90000;
        pkt.data = MakeSample(3570319363U, 1912, 38.2460938F, 39.76416F, 39.9128151F);

        const std::vector<FrameMeta> metas = BuildFrameMeta({pkt}, 90000);

        CU_CHECK_EQ(static_cast<int>(metas.size()), 1);
        if (metas.size() == 1) {
            CU_CHECK_EQ(metas[0].frameIndex, 1);
            CU_CHECK(metas[0].ptsSeconds > 0.999 && metas[0].ptsSeconds < 1.001);
            CU_CHECK_EQ(metas[0].iso, 1912);
            CU_CHECK_EQ(metas[0].timestampNs, 3570319363LL);
            CU_CHECK(metas[0].gainAnalog > 38.24 && metas[0].gainAnalog < 38.25);
            CU_CHECK(metas[0].gainTotal > 39.76 && metas[0].gainTotal < 39.77);
            CU_CHECK(metas[0].gainRedundant > 39.91 && metas[0].gainRedundant < 39.92);
        }
    }

    // BuildFrameMeta: a short sample (missing fields) keeps sentinels.
    {
        MettPacket pkt;
        pkt.pts = 0;
        pkt.data = {0x00, 0x0F, 0xA0, 0x0A}; // too short, no fields

        const std::vector<FrameMeta> metas = BuildFrameMeta({pkt}, 90000);

        CU_CHECK_EQ(static_cast<int>(metas.size()), 1);
        if (metas.size() == 1) {
            CU_CHECK_EQ(metas[0].iso, -1);
            CU_CHECK_EQ(metas[0].timestampNs, -1);
            CU_CHECK(metas[0].gainAnalog < 0.0);
            CU_CHECK(metas[0].gainTotal < 0.0);
            CU_CHECK(metas[0].gainRedundant < 0.0);
        }
    }

    // BuildFrameMeta: gain semantics — analog gain = ISO/50, total gain = analog * digital.
    {
        MettPacket p1;
        p1.pts = 0;
        p1.data = MakeSample(1903704363U, 382, 7.649414F, 7.95283175F, 7.98256254F);

        MettPacket p2;
        p2.pts = 3001;
        p2.data = MakeSample(3836978363U, 1912, 38.2460938F, 39.76416F, 39.9128151F);

        const std::vector<FrameMeta> metas = BuildFrameMeta({p1, p2}, 90000);

        CU_CHECK_EQ(static_cast<int>(metas.size()), 2);
        if (metas.size() == 2) {
            CU_CHECK_EQ(metas[0].iso, 382);
            CU_CHECK_EQ(metas[1].iso, 1912);
            CU_CHECK(!metas[0].isHeader);
            CU_CHECK(!metas[1].isHeader);
            // analog gain = ISO / 50 (within tolerance)
            CU_CHECK(metas[0].gainAnalog > 382.0 / 50.0 - 0.02 && metas[0].gainAnalog < 382.0 / 50.0 + 0.02);
            CU_CHECK(metas[1].gainAnalog > 1912.0 / 50.0 - 0.05 && metas[1].gainAnalog < 1912.0 / 50.0 + 0.05);
        }
    }

    // BuildFrameMeta: the "SAT" header sample is flagged and its fields stay sentinels.
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
            CU_CHECK(metas[0].gainAnalog < 0.0);
        }
    }
}

#endif // TESTS_LIVEPHOTODECODE_LPMETAPARSERTEST_H

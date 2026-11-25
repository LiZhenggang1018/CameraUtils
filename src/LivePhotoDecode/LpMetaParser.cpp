#include "LivePhotoDecode/LpMetaParser.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "FFmpegDefine.h"
#include "LivePhotoDecode/LpDecodeDefine.h"

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kMettCodecTag = 0x7474656d; // "mett"

// Confirmed field offsets (big-endian, relative to the sample start).
constexpr size_t kTimestampNsOffset = 36; // uint32 (nanoseconds)
constexpr size_t kIsoOffset = 42;         // uint16
constexpr size_t kGainAOffset = 60;       // float (analog gain = ISO/50)
constexpr size_t kGainBOffset = 64;       // float (total gain)
constexpr size_t kGainCOffset = 68;       // float (redundant gain)

uint16_t ReadBigEndianU16(const uint8_t* p)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]));
}

uint32_t ReadBigEndianU32(const uint8_t* p)
{
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

float ReadBigEndianFloat(const uint8_t* p)
{
    const uint32_t b0 = p[0];
    const uint32_t b1 = p[1];
    const uint32_t b2 = p[2];
    const uint32_t b3 = p[3];
    const uint32_t bits = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

} // namespace

std::vector<MettPacket> ReadMettPackets(const std::string& mp4Path, int64_t& timeBaseDen)
{
    std::vector<MettPacket> packets;
    timeBaseDen = 90000;

    AVFormatContext* rawFmtCtx = nullptr;
    if (avformat_open_input(&rawFmtCtx, mp4Path.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "[ReadMettPackets] cannot open: " << mp4Path << std::endl;
        return packets;
    }
    AVFormatContextPtr fmtCtx(rawFmtCtx);

    if (avformat_find_stream_info(fmtCtx.get(), nullptr) < 0) {
        std::cerr << "[ReadMettPackets] cannot read stream info" << std::endl;
        return packets;
    }

    int mettIndex = -1;
    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i) {
        AVCodecParameters* par = fmtCtx->streams[i]->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_DATA && par->codec_tag == kMettCodecTag) {
            mettIndex = static_cast<int>(i);
            const int64_t den = fmtCtx->streams[i]->time_base.den;
            if (den > 0) {
                timeBaseDen = den;
            }
            break;
        }
    }
    if (mettIndex < 0) {
        std::cerr << "[ReadMettPackets] no mett data track (codec_tag 0x7474656d)" << std::endl;
        return packets;
    }

    AVPacket* rawPkt = av_packet_alloc();
    AVPacketPtr pkt(rawPkt);
    if (!pkt) {
        return packets;
    }

    while (av_read_frame(fmtCtx.get(), pkt.get()) >= 0) {
        if (pkt->stream_index == mettIndex) {
            MettPacket mp;
            mp.pts = pkt->pts;
            mp.data.assign(pkt->data, pkt->data + pkt->size);
            packets.push_back(std::move(mp));
        }
        av_packet_unref(pkt.get());
    }

    std::cout << "[ReadMettPackets] read " << packets.size() << " mett samples (time_base.den=" << timeBaseDen << ")"
              << std::endl;
    return packets;
}

std::vector<FrameMeta> BuildFrameMeta(const std::vector<MettPacket>& packets, int64_t timeBaseDen)
{
    std::vector<FrameMeta> metas;
    metas.reserve(packets.size());

    int index = 1;
    for (const auto& p : packets) {
        FrameMeta m;
        m.frameIndex = index;
        m.ptsSeconds = (timeBaseDen > 0) ? (static_cast<double>(p.pts) / static_cast<double>(timeBaseDen)) : 0.0;

        // The first sample is a "SAT" structure header (data[40..42] == "SAT");
        // it carries stream metadata, not per-frame exposure fields.
        if (p.data.size() > kGainCOffset + 3) {
            const bool isSatHeader =
                (p.data.size() > 42) && p.data[40] == 'S' && p.data[41] == 'A' && p.data[42] == 'T';
            m.isHeader = isSatHeader;
            if (!isSatHeader) {
                m.timestampNs = static_cast<int64_t>(ReadBigEndianU32(p.data.data() + kTimestampNsOffset));
                m.iso = ReadBigEndianU16(p.data.data() + kIsoOffset);
                m.gainAnalog = ReadBigEndianFloat(p.data.data() + kGainAOffset);
                m.gainTotal = ReadBigEndianFloat(p.data.data() + kGainBOffset);
                m.gainRedundant = ReadBigEndianFloat(p.data.data() + kGainCOffset);
            }
        }

        metas.push_back(std::move(m));
        ++index;
    }

    return metas;
}

void ExtractFrameMeta()
{
    const std::string& mp4Path = LpInputMp4Path();
    const std::string& outDir = LpOutputDir();
    const std::string outTxt = outDir + "/frame_meta.txt";

    std::error_code ec;
    if (!fs::exists(outDir) && !fs::create_directories(outDir, ec)) {
        std::cerr << "[ExtractFrameMeta] cannot create output dir: " << outDir << std::endl;
        return;
    }

    int64_t timeBaseDen = 90000;
    const std::vector<MettPacket> packets = ReadMettPackets(mp4Path, timeBaseDen);
    if (packets.empty()) {
        std::cerr << "[ExtractFrameMeta] no mett samples, skip" << std::endl;
        return;
    }

    const std::vector<FrameMeta> metas = BuildFrameMeta(packets, timeBaseDen);

    std::ofstream ofs(outTxt);
    if (!ofs.is_open()) {
        std::cerr << "[ExtractFrameMeta] cannot write file: " << outTxt << std::endl;
        return;
    }

    ofs << "# frameIndex pts_seconds iso timestamp_ns gain_analog gain_total gain_redundant\n";
    for (const auto& m : metas) {
        if (m.isHeader) {
            ofs << m.frameIndex << " " << m.ptsSeconds << " header\n";
            continue;
        }
        ofs << m.frameIndex << " " << m.ptsSeconds << " " << m.iso << " " << m.timestampNs << " " << m.gainAnalog << " "
            << m.gainTotal << " " << m.gainRedundant << "\n";
    }

    std::cout << "[ExtractFrameMeta] wrote " << metas.size() << " frames meta -> " << outTxt << std::endl;
}

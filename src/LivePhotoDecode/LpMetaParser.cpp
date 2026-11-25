#include "LivePhotoDecode/LpMetaParser.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <utility>

#include "Exiv2Common.h"
#include "FFmpegDefine.h"
#include "LivePhotoDecode/LpDecodeDefine.h"

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kMettCodecTag = 0x7474656d; // "mett"

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

std::vector<float> ParseBigEndianFloats(const std::vector<uint8_t>& data)
{
    std::vector<float> result;
    result.reserve(data.size() / 4);
    for (size_t i = 0; i + 3 < data.size(); ++i) {
        const float f = ReadBigEndianFloat(data.data() + i);
        if (std::isnan(f) || std::isinf(f)) {
            continue;
        }
        const float a = std::fabs(f);
        if (a < 1e-4F || a > 1e6F) {
            continue;
        }
        result.push_back(f);
    }
    return result;
}

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

std::vector<FrameMeta> BuildFrameMeta(const std::vector<MettPacket>& packets, int64_t timeBaseDen, double coverExposure,
                                      double coverIso)
{
    std::vector<FrameMeta> metas;
    metas.reserve(packets.size());

    int index = 1;
    for (const auto& p : packets) {
        FrameMeta m;
        m.frameIndex = index;
        m.ptsSeconds = (timeBaseDen > 0) ? (static_cast<double>(p.pts) / static_cast<double>(timeBaseDen)) : 0.0;
        m.rawFloats = ParseBigEndianFloats(p.data);

        // Anchor: find the value in the sample floats closest to the cover truth (±2% relative tolerance).
        const double expTol = (coverExposure > 0.0) ? coverExposure * 0.02 : 0.0;
        const double isoTol = (coverIso > 0.0) ? coverIso * 0.02 : 0.0;
        double bestExpDiff = std::numeric_limits<double>::max();
        double bestIsoDiff = std::numeric_limits<double>::max();

        for (const float f : m.rawFloats) {
            const double d = static_cast<double>(f);
            if (coverExposure > 0.0) {
                const double diff = std::fabs(d - coverExposure);
                if (diff <= expTol && diff < bestExpDiff) {
                    bestExpDiff = diff;
                    m.exposureTime = d;
                }
            }
            if (coverIso > 0.0) {
                const double diff = std::fabs(d - coverIso);
                if (diff <= isoTol && diff < bestIsoDiff) {
                    bestIsoDiff = diff;
                    m.iso = d;
                }
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
    const std::string& jpgPath = LpInputJpgPath();
    const std::string& outDir = LpOutputDir();
    const std::string outTxt = outDir + "/frame_meta.txt";

    std::error_code ec;
    if (!fs::exists(outDir) && !fs::create_directories(outDir, ec)) {
        std::cerr << "[ExtractFrameMeta] cannot create output dir: " << outDir << std::endl;
        return;
    }

    const CameraUtils::ExposureInfo expInfo = CameraUtils::ReadExposureInfo(jpgPath);

    int64_t timeBaseDen = 90000;
    const std::vector<MettPacket> packets = ReadMettPackets(mp4Path, timeBaseDen);
    if (packets.empty()) {
        std::cerr << "[ExtractFrameMeta] no mett samples, skip" << std::endl;
        return;
    }

    const std::vector<FrameMeta> metas = BuildFrameMeta(packets, timeBaseDen, expInfo.timeSeconds, expInfo.iso);

    std::ofstream ofs(outTxt);
    if (!ofs.is_open()) {
        std::cerr << "[ExtractFrameMeta] cannot write file: " << outTxt << std::endl;
        return;
    }

    ofs << "# frameIndex pts_seconds exposure_time(sec,speculative) iso(speculative)\n";
    for (const auto& m : metas) {
        ofs << m.frameIndex << " " << m.ptsSeconds << " " << m.exposureTime << " " << m.iso;
        if (!m.rawFloats.empty()) {
            ofs << " raw=";
            const size_t show = std::min<size_t>(m.rawFloats.size(), 8);
            for (size_t i = 0; i < show; ++i) {
                if (i != 0) {
                    ofs << ",";
                }
                ofs << m.rawFloats[i];
            }
        }
        ofs << "\n";
    }

    std::cout << "[ExtractFrameMeta] wrote " << metas.size() << " frames meta -> " << outTxt << std::endl;
}

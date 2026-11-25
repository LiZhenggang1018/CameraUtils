#include "LivePhotoDecode/LpMetaParser.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <utility>

#include "Exiv2Common.h"
#include "FFmpegDefine.h"
#include "LivePhotoDecode/LpDecodeDefine.h"

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

std::vector<MettPacket> ReadMettPackets(const std::string& mp4Path)
{
    std::vector<MettPacket> packets;

    AVFormatContext* rawFmtCtx = nullptr;
    if (avformat_open_input(&rawFmtCtx, mp4Path.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "[ReadMettPackets] 无法打开: " << mp4Path << std::endl;
        return packets;
    }
    AVFormatContextPtr fmtCtx(rawFmtCtx);

    if (avformat_find_stream_info(fmtCtx.get(), nullptr) < 0) {
        std::cerr << "[ReadMettPackets] 无法读取流信息" << std::endl;
        return packets;
    }

    int mettIndex = -1;
    int64_t timeBaseDen = 90000;
    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i) {
        AVCodecParameters* par = fmtCtx->streams[i]->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_DATA && par->codec_tag == kMettCodecTag) {
            mettIndex = static_cast<int>(i);
            timeBaseDen = fmtCtx->streams[i]->time_base.den;
            if (timeBaseDen <= 0) {
                timeBaseDen = 90000;
            }
            break;
        }
    }
    if (mettIndex < 0) {
        std::cerr << "[ReadMettPackets] 未找到 mett 数据轨道 (codec_tag 0x7474656d)" << std::endl;
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

    std::cout << "[ReadMettPackets] 读取 " << packets.size() << " 个 mett 样本 (time_base.den=" << timeBaseDen << ")"
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

        // 锚定：在样本 float 里找与封面真值最接近的值（±2% 相对容差）。
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
    const std::string outTxt = LpOutputDir() + "/frame_meta.txt";

    const CameraUtils::ExposureInfo expInfo = CameraUtils::ReadExposureInfo(jpgPath);
    // ISO 与封面 EXIF 同源：ReadExposureInfo 对 vivo 的 PhotographicSensitivity 失效，
    // 这里与 ExtractCoverExif 一样回退读 ISOSpeedRatings。
    double coverIso = -1.0;
    try {
        auto image = Exiv2::ImageFactory::open(jpgPath);
        if (image.get()) {
            image->readMetadata();
            const auto& exif = image->exifData();
            auto it = exif.findKey(Exiv2::ExifKey("Exif.Photo.ISOSpeedRatings"));
            if (it == exif.end()) {
                it = exif.findKey(Exiv2::ExifKey("Exif.Photo.RecommendedExposureIndex"));
            }
            if (it != exif.end()) {
                coverIso = static_cast<double>(it->toFloat());
            }
        }
    } catch (const Exiv2::Error& e) {
        std::cerr << "[ExtractFrameMeta] 读取封面 ISO 失败: " << e.what() << std::endl;
    }

    const std::vector<MettPacket> packets = ReadMettPackets(mp4Path);
    if (packets.empty()) {
        std::cerr << "[ExtractFrameMeta] 无 mett 样本，跳过" << std::endl;
        return;
    }

    const std::vector<FrameMeta> metas = BuildFrameMeta(packets, 90000, expInfo.timeSeconds, coverIso);

    std::ofstream ofs(outTxt);
    if (!ofs.is_open()) {
        std::cerr << "[ExtractFrameMeta] 无法写文件: " << outTxt << std::endl;
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

    std::cout << "[ExtractFrameMeta] 写出 " << metas.size() << " 帧 meta -> " << outTxt << std::endl;
}

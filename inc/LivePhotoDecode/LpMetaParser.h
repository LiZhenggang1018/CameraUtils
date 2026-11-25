#ifndef INC_LIVEPHOTODECODE_LPMETAPARSER_H
#define INC_LIVEPHOTODECODE_LPMETAPARSER_H

#include <cstdint>
#include <string>
#include <vector>

// ============================================================
// vivo LivePhoto 随帧 meta（mett 轨道）解析。
//
// 轨道结构（实测）：77 个样本，每个样本以 4 字节 tag `00 0F A0 0A` 开头，
// 后接 4 字节 big-endian 总长度，再跟 payload（内含嵌套 TLV 与 IEEE754 float）。
// 字段语义未公开；时间戳由 mett 流的 PTS（time_base 1/90000）可靠给出，
// 曝光/ISO 若无法以普通 float 匹配封面 EXIF 真值则输出 -1 并保留原始 float。
// ============================================================

struct MettPacket {
    int64_t pts;               // mett 流 time_base（1/90000）下的 PTS
    std::vector<uint8_t> data; // 原始样本字节（含 tag 头）
};

struct FrameMeta {
    int frameIndex = 0;           // 1-based
    double ptsSeconds = 0.0;      // pts / time_base.den
    double exposureTime = -1.0;   // 推测曝光时间（秒），未匹配为 -1
    double iso = -1.0;            // 推测 ISO，未匹配为 -1
    std::vector<float> rawFloats; // 样本内解析出的 float 原始值（保留待查）
};

// 读 mett 轨道（stream 中 codec_tag == 0x7474656d 的数据流）全部样本。
std::vector<MettPacket> ReadMettPackets(const std::string& mp4Path);

// 从字节序列逐 4 字节 big-endian 扫描 IEEE754 float，过滤 NaN/Inf 与过噪值。
std::vector<float> ParseBigEndianFloats(const std::vector<uint8_t>& data);

// 用封面 EXIF 真值锚定曝光时间/ISO；对不上的字段保持 -1。
std::vector<FrameMeta> BuildFrameMeta(const std::vector<MettPacket>& packets, int64_t timeBaseDen, double coverExposure,
                                      double coverIso);

// 解析并写出 output/frame_meta.txt。
void ExtractFrameMeta();

#endif // INC_LIVEPHOTODECODE_LPMETAPARSER_H

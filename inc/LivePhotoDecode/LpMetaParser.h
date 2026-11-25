#ifndef INC_LIVEPHOTODECODE_LPMETAPARSER_H
#define INC_LIVEPHOTODECODE_LPMETAPARSER_H

#include <cstdint>
#include <string>
#include <vector>

// ============================================================
// vivo LivePhoto per-frame metadata (mett track) parsing.
//
// Track layout (observed): 77 samples, each starting with a 4-byte tag
// `00 0F A0 0A` followed by a 4-byte big-endian total length, then a payload
// (nested TLV + IEEE754 floats). Field semantics are unpublished; timestamps
// come reliably from the mett stream PTS (time_base 1/90000), while exposure/ISO
// output -1 and keep raw floats when they cannot be matched as plain floats.
// ============================================================

struct MettPacket {
    int64_t pts;               // PTS in the mett stream time_base (1/90000)
    std::vector<uint8_t> data; // raw sample bytes (including tag header)
};

struct FrameMeta {
    int frameIndex = 0;           // 1-based
    double ptsSeconds = 0.0;      // pts / time_base.den
    double exposureTime = -1.0;   // speculative exposure time (seconds), -1 if unmatched
    double iso = -1.0;            // speculative ISO, -1 if unmatched
    std::vector<float> rawFloats; // raw floats parsed from the sample (kept for inspection)
};

// Read all samples from the mett track (the data stream with codec_tag == 0x7474656d).
// timeBaseDen outputs the mett stream's time_base.den (used for pts-to-seconds conversion).
std::vector<MettPacket> ReadMettPackets(const std::string& mp4Path, int64_t& timeBaseDen);

// Scan a byte sequence byte-by-byte as big-endian IEEE754 floats, filtering NaN/Inf and noise.
std::vector<float> ParseBigEndianFloats(const std::vector<uint8_t>& data);

// Anchor exposure time/ISO to the cover EXIF truth values; unmatched fields stay -1.
std::vector<FrameMeta> BuildFrameMeta(const std::vector<MettPacket>& packets, int64_t timeBaseDen, double coverExposure,
                                      double coverIso);

// Parse and write output/frame_meta.txt.
void ExtractFrameMeta();

#endif // INC_LIVEPHOTODECODE_LPMETAPARSER_H

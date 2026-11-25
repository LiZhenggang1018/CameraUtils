#ifndef INC_LIVEPHOTODECODE_LPMETAPARSER_H
#define INC_LIVEPHOTODECODE_LPMETAPARSER_H

#include <cstdint>
#include <string>
#include <vector>

// ============================================================
// vivo LivePhoto per-frame metadata (mett track) parsing.
//
// Track layout (reverse-engineered, big-endian): 77 samples, each starting
// with a 4-byte tag `00 0F A0 0A` + 4-byte total length + payload. Confirmed
// per-frame fields (offsets are relative to the sample start):
//
//   +42  uint16  ISO (video recording ISO; jumps 382 -> 1912 at the point
//                where the high-ISO cover photo is embedded into the stream)
//   +60  float   exposure-related value (scales 5x along with ISO)
//   +64  float   exposure-related value (scales 5x along with ISO)
//   +68  float   exposure-related value (scales 5x along with ISO)
//
// Timestamps come reliably from the mett stream PTS (time_base 1/90000).
// The +60/+64/+68 values are NOT yet unit-confirmed (speculative); they are
// kept raw for later calibration against a stopwatch-based capture.
// ============================================================

struct MettPacket {
    int64_t pts;               // PTS in the mett stream time_base (1/90000)
    std::vector<uint8_t> data; // raw sample bytes (including tag header)
};

struct FrameMeta {
    int frameIndex = 0;         // 1-based
    double ptsSeconds = 0.0;    // pts / time_base.den
    bool isHeader = false;      // true for the "SAT" structure header sample (no per-frame fields)
    int iso = -1;               // confirmed ISO (uint16 at +42), -1 for the header sample
    double shutterApexA = -1.0; // speculative exposure value (float at +60)
    double shutterApexB = -1.0; // speculative exposure value (float at +64)
    double shutterApexC = -1.0; // speculative exposure value (float at +68)
};

// Read all samples from the mett track (the data stream with codec_tag == 0x7474656d).
// timeBaseDen outputs the mett stream's time_base.den (used for pts-to-seconds conversion).
std::vector<MettPacket> ReadMettPackets(const std::string& mp4Path, int64_t& timeBaseDen);

// Parse the confirmed per-frame fields (ISO at +42, floats at +60/+64/+68)
// from each sample. Does not do byte-by-byte scanning.
std::vector<FrameMeta> BuildFrameMeta(const std::vector<MettPacket>& packets, int64_t timeBaseDen);

// Parse and write output/frame_meta.txt.
void ExtractFrameMeta();

#endif // INC_LIVEPHOTODECODE_LPMETAPARSER_H

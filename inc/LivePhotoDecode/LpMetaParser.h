#ifndef INC_LIVEPHOTODECODE_LPMETAPARSER_H
#define INC_LIVEPHOTODECODE_LPMETAPARSER_H

#include <cstdint>
#include <string>
#include <vector>

// ============================================================
// vivo LivePhoto per-frame metadata (mett track) parsing.
//
// Track layout (reverse-engineered, big-endian): one "SAT" header sample
// followed by N data samples, each starting with a 4-byte tag `00 0F A0 0A`
// + 4-byte total length + payload. Confirmed per-frame fields (offsets are
// relative to the sample start), calibrated against two real captures:
//
//   +36  uint32  nanosecond timestamp (frame spacing 33 333 300 ns = 1/30 s)
//   +42  uint16  ISO
//   +60  float   analog gain = ISO / 50 (50 = sensor base ISO)
//   +64  float   total gain = analog gain * digital gain
//   +68  float   redundant gain expression (= total gain)
//
// The +60/+64/+68 gain semantics were confirmed by APEX cross-validation:
// +60/ISO == 0.02 exactly across ISO 251/382/1912; +64/+60 == digital gain
// (1.0 this capture, 1.0397 the other). Shutter speed is NOT one of these
// fields; it was constant in both captures and so did not expose itself.
// ============================================================

struct MettPacket {
    int64_t pts;               // PTS in the mett stream time_base (1/90000)
    std::vector<uint8_t> data; // raw sample bytes (including tag header)
};

struct FrameMeta {
    int frameIndex = 0;          // 1-based
    double ptsSeconds = 0.0;     // pts / time_base.den
    bool isHeader = false;       // true for the "SAT" structure header sample (no per-frame fields)
    int iso = -1;                // confirmed ISO (uint16 at +42), -1 for the header sample
    int64_t timestampNs = -1;    // confirmed nanosecond timestamp (uint32 at +36)
    double gainAnalog = -1.0;    // analog gain = ISO / 50 (float at +60)
    double gainTotal = -1.0;     // total gain (float at +64)
    double gainRedundant = -1.0; // redundant gain expression (float at +68)
};

// Read all samples from the mett track (the data stream with codec_tag == 0x7474656d).
// timeBaseDen outputs the mett stream's time_base.den (used for pts-to-seconds conversion).
std::vector<MettPacket> ReadMettPackets(const std::string& mp4Path, int64_t& timeBaseDen);

// Parse the confirmed per-frame fields (timestamp at +36, ISO at +42,
// gains at +60/+64/+68) from each sample. Does not do byte-by-byte scanning.
std::vector<FrameMeta> BuildFrameMeta(const std::vector<MettPacket>& packets, int64_t timeBaseDen);

// Parse and write output/frame_meta.txt.
void ExtractFrameMeta();

#endif // INC_LIVEPHOTODECODE_LPMETAPARSER_H

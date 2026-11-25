#ifndef INC_LIVEPHOTODECODE_LPDECODE_H
#define INC_LIVEPHOTODECODE_LPDECODE_H

#include "LivePhotoDecode/LpDecodeDefine.h"

// Extract the cover frame: read the cover JPG, write output/cover.jpg and output/cover.yuv (YUV420P).
void ExtractCoverFrame();

// Read the cover EXIF, write output/cover_exif.txt (exposure time / ISO / DateTimeOriginal).
void ExtractCoverExif();

// Decode the live stream: FFmpeg frame-by-frame decode to output/video/{N}.yuv + {N}.jpg.
void ExtractLiveStream();

#endif // INC_LIVEPHOTODECODE_LPDECODE_H

#ifndef INC_LIVEPHOTODECODE_LPDECODE_H
#define INC_LIVEPHOTODECODE_LPDECODE_H

#include "LivePhotoDecode/LpDecodeDefine.h"

// 提取封面帧：读封面 JPG，写 output/cover.jpg 与 output/cover.yuv（YUV420P）
void ExtractCoverFrame();

// 读取封面 EXIF，写 output/cover_exif.txt（曝光时间 / ISO / DateTimeOriginal）
void ExtractCoverExif();

#endif // INC_LIVEPHOTODECODE_LPDECODE_H

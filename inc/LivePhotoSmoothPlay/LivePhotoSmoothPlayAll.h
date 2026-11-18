#ifndef INC_LIVEPHOTOSMOOTHPLAY_LIVEPHOTOSMOOTHPLAYALL_H
#define INC_LIVEPHOTOSMOOTHPLAY_LIVEPHOTOSMOOTHPLAYALL_H

// LivePhoto 平滑播放模块聚合头：依赖 FFmpeg 与 OpenCV。
#include "FFMpegCommon.h"
#include "OpenCVCommon.h"

#include "AlphaMergeYuvs.h"
#include "CenterCropGenYuvs.h"
#include "DecAndEnc.h"
#include "DecDemo.h"
#include "DumpUtils.h"
#include "FFmpegDefine.h"
#include "GenMp4.h"
#include "ImageRegistrator.h"
#include "LpFovTransitionOpt.h"
#include "LpReg.h"
#include "YUVImageReader.h"
#include "YUVProcessor.h"

#endif // INC_LIVEPHOTOSMOOTHPLAY_LIVEPHOTOSMOOTHPLAYALL_H

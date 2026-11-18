#ifndef LIVEPHOTOSMOOTHPLAYALL_H
#define LIVEPHOTOSMOOTHPLAYALL_H

// LivePhoto 平滑播放模块聚合头：依赖 FFmpeg 与 OpenCV。
#include "OpenCVCommon.h"
#include "FFMpegCommon.h"

#include "FFmpegDefine.h"
#include "DumpUtils.h"
#include "GenMp4.h"
#include "YUVProcessor.h"
#include "YUVImageReader.h"
#include "ImageRegistrator.h"
#include "AlphaMergeYuvs.h"
#include "LpReg.h"
#include "DecAndEnc.h"
#include "CenterCropGenYuvs.h"
#include "DecDemo.h"
#include "LpFovTransitionOpt.h"

#endif // LIVEPHOTOSMOOTHPLAYALL_H

#ifndef OPTICALFLOWALL_H
#define OPTICALFLOWALL_H

// 光流 / 配准模块聚合头：依赖 OpenCV 与 FFmpeg(VideoWriter)。
#include "OpenCVCommon.h"

#include "HarrisDetect.h"

#include "LkTracker.h"
#include "MiddleburyColor.h"
#include "Visualization.h"
#include "DenseFlow.h"
#include "BiDirectionalLkFilter.h"
#include "FrameInterpolator.h"
#include "DenseFlowDeepFlow.h"
#include "DenseFlowDis.h"
#include "FfmpegVideoWriter.h"
#include "InterpolationConfig.h"

#endif // OPTICALFLOWALL_H

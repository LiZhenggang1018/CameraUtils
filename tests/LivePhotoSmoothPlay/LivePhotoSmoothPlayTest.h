#ifndef LIVEPHOTOSMOOTHPLAYTEST_H_
#define LIVEPHOTOSMOOTHPLAYTEST_H_

#include "LivePhotoSmoothPlay/LpFovTransitionOpt.h"
#include "LivePhotoSmoothPlay/ImageRegistrator.h"


inline void RunLivePhotoSmoothPlayTest()
{
    LpFovTransitionOptTest();
}

inline void RegTest()
{
    int rc = RegYuv(
        "data/LivePhotoSmoothPlay/LpJpg/296.yuv",   // 目标图（封面帧）
        "data/LivePhotoSmoothPlay/LpJpg/275.yuv",   // 参考图（最后视频帧）
        "data/LivePhotoSmoothPlay/reg_yuv_demo",    // 输出目录
        720, 1584,                                   // 分辨率
        FeatureDetectorType::ORB,
        TransformType::HOMOGRAPHY);
    std::cout << "RegYuv return code = " << rc << std::endl;

    int rc = RegImg(
    "data/LivePhotoSmoothPlay/cover.jpg",      // 目标图
    "data/LivePhotoSmoothPlay/last_frame.jpg", // 参考图
    "data/LivePhotoSmoothPlay/reg_img_demo",
    FeatureDetectorType::SIFT,                 // 可以切到 SIFT
    TransformType::HOMOGRAPHY);
    std::cout << "RegImg return code = " << rc << std::endl;
}

#endif

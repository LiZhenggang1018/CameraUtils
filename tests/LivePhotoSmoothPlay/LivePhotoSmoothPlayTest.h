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
    // ------------------------------------------------------------
    // 1. YUV 配准：封面帧(296.yuv) 配准到 最后视频帧(275.yuv)
    //    直接复用 FFmpegDefine.h 里已有的绝对路径宏，
    //    避免手写相对路径依赖 CWD。
    // ------------------------------------------------------------
    const std::string refYuvPath = LAST_VID_FRAME_YUV;   // = .../LpJpg/275.yuv
    const std::string curYuvPath = COVER_FRAME_YUV;      // = .../LpJpg/296.yuv
    const std::string yuvOutDir  = LP_SMOOTH_PLAY_DATA_PATH + "reg_yuv_demo";

    std::cout << "[RegTest] refYuv = " << refYuvPath << std::endl;
    std::cout << "[RegTest] curYuv = " << curYuvPath << std::endl;
    std::cout << "[RegTest] outDir = " << yuvOutDir  << std::endl;

    int rc = RegYuv(
        curYuvPath,
        refYuvPath,
        yuvOutDir,
        720, 1584,
        FeatureDetectorType::ORB,
        TransformType::HOMOGRAPHY);
    std::cout << "RegYuv return code = " << rc << std::endl;

    // ------------------------------------------------------------
    // 2. 图像配准：cover.jpg 配准到 last_frame.jpg
    //    图片文件也同样放在 LP_SMOOTH_PLAY_DATA_PATH 下，用绝对路径。
    // ------------------------------------------------------------
    const std::string refImgPath = LP_SMOOTH_PLAY_DATA_PATH + "last_frame.jpg";
    const std::string curImgPath = LP_SMOOTH_PLAY_DATA_PATH + "cover.jpg";
    const std::string imgOutDir  = LP_SMOOTH_PLAY_DATA_PATH + "reg_img_demo";

    std::cout << "[RegTest] refImg = " << refImgPath << std::endl;
    std::cout << "[RegTest] curImg = " << curImgPath << std::endl;
    std::cout << "[RegTest] outDir = " << imgOutDir << std::endl;

    int rc2 = RegImg(
        curImgPath,
        refImgPath,
        imgOutDir,
        FeatureDetectorType::SIFT,
        TransformType::HOMOGRAPHY);
    std::cout << "RegImg return code = " << rc2 << std::endl;
}

#endif

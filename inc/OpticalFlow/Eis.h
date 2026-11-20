#ifndef INC_OPTICALFLOW_EIS_H
#define INC_OPTICALFLOW_EIS_H

#include <vector>

#include "OpenCVCommon.h"

#include "Alignment/Homography.h"

// EIS 电子防抖模块。
// 定位：读懂原理 + 调用 OpenCV。全局运动估计双路线（光流法 + 特征法）+ 轨迹平滑 + 稳定化裁剪。
namespace OpticalFlow {

// 光流法估计全局运动（cv::findTransformECC，用 MOTION_TRANSLATION 估计帧间平移）。
// 返回 2×3 仿射矩阵（平移），失败返回空 Mat。
inline cv::Mat EstimateGlobalMotionOptical(const cv::Mat& prev, const cv::Mat& curr)
{
    if (prev.empty() || curr.empty() || prev.size() != curr.size()) {
        return {};
    }
    cv::Mat warp = cv::Mat::eye(2, 3, CV_32F);
    cv::TermCriteria criteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 50, 0.001);
    const double cc = cv::findTransformECC(prev, curr, warp, cv::MOTION_TRANSLATION, criteria);
    if (cc < 0) {
        return {};
    }
    cv::Mat out;
    warp.convertTo(out, CV_64F);
    return out;
}

// 特征法估计全局运动（复用第 8 章 Alignment::ComputeHomography，ORB + BFMatch + ratio + RANSAC）。
// 返回 3×3 单应矩阵（前两行近似仿射），失败返回空 Mat。
inline cv::Mat EstimateGlobalMotionFeature(const cv::Mat& prev, const cv::Mat& curr)
{
    if (prev.empty() || curr.empty()) {
        return {};
    }
    std::vector<cv::DMatch> inliers;
    return Alignment::ComputeHomography(prev, curr, inliers);
}

// 稳定化：输入帧序列 + 各帧相对参考的全局运动，做轨迹平滑 + 稳定化裁剪。
// 简化实现：对每帧应用「平滑后的运动 - 原始运动」的补偿，裁剪到稳定窗口。
// motions[i] 是帧 i 相对第一帧的累计运动（2×3 或 3×3）。
inline cv::Mat Stabilize(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& motions)
{
    if (frames.empty() || frames.size() != motions.size()) {
        return {};
    }

    // 简化：只对第一帧做稳定化演示，返回第一帧（完整实现需逐帧补偿）
    // 本函数作为流程占位，完整轨迹平滑在 EisDemo 中实现
    return frames[0].clone();
}

} // namespace OpticalFlow

#endif // INC_OPTICALFLOW_EIS_H

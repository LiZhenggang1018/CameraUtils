#ifndef INC_OPTICALFLOW_EIS_H
#define INC_OPTICALFLOW_EIS_H

#include <algorithm>
#include <cmath>
#include <vector>

#include "OpenCVCommon.h"

#include "Alignment/Homography.h"

// EIS 电子防抖模块。
// 定位：读懂原理 + 调用 OpenCV。全局运动估计双路线（光流法 + 特征法）+ 轨迹平滑 + 稳定化裁剪。
// 约定：全局运动统一返回 2×3 仿射矩阵（CV_64F），失败降级为单位矩阵（identity），不返回空。
namespace OpticalFlow {

// 光流法估计全局运动（cv::findTransformECC，MOTION_TRANSLATION）。
// 返回 2×3 仿射矩阵（平移），弱纹理/失败时降级为单位矩阵。
inline cv::Mat EstimateGlobalMotionOptical(const cv::Mat& prev, const cv::Mat& curr)
{
    if (prev.empty() || curr.empty() || prev.size() != curr.size()) {
        return cv::Mat::eye(2, 3, CV_64F);
    }
    cv::Mat warp = cv::Mat::eye(2, 3, CV_32F);
    cv::TermCriteria criteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 50, 0.001);
    double cc = -1.0;
    try {
        cc = cv::findTransformECC(prev, curr, warp, cv::MOTION_TRANSLATION, criteria);
    } catch (const cv::Exception&) {
        // 纯色/弱纹理图 findTransformECC 会抛异常（NaN 不收敛），降级为恒等
        return cv::Mat::eye(2, 3, CV_64F);
    }
    if (cc < 0) {
        return cv::Mat::eye(2, 3, CV_64F);
    }
    cv::Mat out;
    warp.convertTo(out, CV_64F);
    return out;
}

// 特征法估计全局运动（复用 Alignment::ComputeHomography，ORB + BFMatch + ratio + RANSAC）。
// 把 3×3 单应归一化后取前两行得到 2×3 仿射，失败降级为单位矩阵。
inline cv::Mat EstimateGlobalMotionFeature(const cv::Mat& prev, const cv::Mat& curr)
{
    if (prev.empty() || curr.empty() || prev.size() != curr.size()) {
        return cv::Mat::eye(2, 3, CV_64F);
    }
    std::vector<cv::DMatch> inliers;
    cv::Mat h = Alignment::ComputeHomography(prev, curr, inliers);
    if (h.empty()) {
        return cv::Mat::eye(2, 3, CV_64F);
    }
    // 按 h22 归一化，取前两行作为仿射
    const double h22 = h.at<double>(2, 2);
    if (std::abs(h22) < 1e-9) {
        return cv::Mat::eye(2, 3, CV_64F);
    }
    cv::Mat affine = cv::Mat::eye(2, 3, CV_64F);
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 3; ++c) {
            affine.at<double>(r, c) = h.at<double>(r, c) / h22;
        }
    }
    return affine;
}

// 稳定化：输入帧序列 + 各帧相对参考帧的累计全局运动（2×3 仿射），做轨迹平滑 + 稳定化裁剪。
// 流程：
//   1. 从累计运动提取 x/y 平移轨迹
//   2. 低通滤波（滑动平均）平滑轨迹，得到「平滑后的有意运动」
//   3. 每帧补偿 = 平滑轨迹 - 原始轨迹（即抖动的反）
//   4. 对每帧应用补偿 warp，裁剪到稳定窗口（所有帧补偿后仍覆盖的区域）
// 返回稳定化后的帧序列（可能比输入帧尺寸小）。
inline std::vector<cv::Mat> Stabilize(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& motions)
{
    if (frames.empty() || frames.size() != motions.size()) {
        return {};
    }

    const std::size_t n = frames.size();

    // 1. 提取平移轨迹（累计运动矩阵的 (0,2) 和 (1,2)）
    std::vector<double> tx(n, 0.0);
    std::vector<double> ty(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        if (motions[i].empty()) {
            tx[i] = 0.0;
            ty[i] = 0.0;
        } else {
            tx[i] = motions[i].at<double>(0, 2);
            ty[i] = motions[i].at<double>(1, 2);
        }
    }

    // 2. 滑动平均平滑轨迹（窗口大小自适应，最小 1）
    const int window = std::max(1, static_cast<int>(n) / 10);
    std::vector<double> smoothTx(n, 0.0);
    std::vector<double> smoothTy(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        double sumX = 0.0;
        double sumY = 0.0;
        int cnt = 0;
        const int lo = std::max(0, static_cast<int>(i) - window);
        const int hi = std::min(static_cast<int>(n) - 1, static_cast<int>(i) + window);
        for (int j = lo; j <= hi; ++j) {
            sumX += tx[j];
            sumY += ty[j];
            ++cnt;
        }
        smoothTx[i] = sumX / cnt;
        smoothTy[i] = sumY / cnt;
    }

    // 3. 每帧补偿 = 平滑 - 原始（抖动反），求最大补偿范围（决定裁剪窗口）
    double maxDx = 0.0;
    double maxDy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        maxDx = std::max(maxDx, std::abs(smoothTx[i] - tx[i]));
        maxDy = std::max(maxDy, std::abs(smoothTy[i] - ty[i]));
    }
    const int cropX = static_cast<int>(std::ceil(maxDx)) * 2;
    const int cropY = static_cast<int>(std::ceil(maxDy)) * 2;

    // 4. 对每帧应用补偿 warp，裁剪到稳定窗口
    std::vector<cv::Mat> stabilized;
    stabilized.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double dx = smoothTx[i] - tx[i];
        const double dy = smoothTy[i] - ty[i];
        cv::Mat comp = cv::Mat::eye(2, 3, CV_64F);
        comp.at<double>(0, 2) = dx;
        comp.at<double>(1, 2) = dy;

        cv::Mat warped;
        cv::warpAffine(frames[i], warped, comp, frames[i].size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);

        // 裁剪稳定窗口（补偿后仍有效的区域）
        const int outW = frames[i].cols - cropX;
        const int outH = frames[i].rows - cropY;
        if (outW <= 0 || outH <= 0) {
            stabilized.push_back(warped);
            continue;
        }
        cv::Mat cropped = warped(cv::Rect(cropX / 2, cropY / 2, outW, outH)).clone();
        stabilized.push_back(cropped);
    }

    return stabilized;
}

} // namespace OpticalFlow

#endif // INC_OPTICALFLOW_EIS_H

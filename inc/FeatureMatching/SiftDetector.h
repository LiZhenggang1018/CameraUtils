#ifndef INC_FEATUREMATCHING_SIFTDETECTOR_H
#define INC_FEATUREMATCHING_SIFTDETECTOR_H

#include <vector>

#include "OpenCVCommon.h"

// SIFT（Scale-Invariant Feature Transform）手写实现。
//
// 定位：打破「只调 OpenCV」的约束，手写 SIFT 四阶段核心逻辑
//   （DoG 尺度空间 / 极值检测与亚像素定位 / 方向分配 / 128 维描述子），
//   基础算子（GaussianBlur / Sobel / resize）仍复用 OpenCV。
//
// 输出与 cv::SIFT 对齐：keypoints 的 pt 为原图坐标，descriptors 为 CV_32F Nx128，
//   可直接用 cv::BFMatcher(NORM_L2) 与 cv::drawKeypoints，便于与 cv::SIFT 交叉验证。
//
// 输入要求：CV_8UC1 单通道灰度图；空输入安全返回空。
namespace FeatureMatching {

class SiftDetector {
public:
    // SIFT 可调参数（对齐 Lowe 论文默认值）。
    struct Params {
        int nOctaveLayers = 3;          // 每个 octave 的间隔数（interval），影响尺度采样密度
        double contrastThreshold = 1.0; // 低对比度关键点剔除阈值（DoG 响应绝对值；本实现未归一化，故高于 Lowe 的 0.04）
        double edgeThreshold = 10.0;    // 边缘响应剔除的曲率比阈值 r
        double sigma = 1.6;             // 初始尺度（尺度空间基础高斯 sigma）
        int nmsWindowSize = 5;          // 空间非极大值抑制窗口边长（奇数），值越大保留越少
        int nfeatures = 0;              // 保留的最强关键点数上限（按响应排序取前 N；0 表示不限制）
    };

    SiftDetector()
        : SiftDetector(Params{})
    {}

    explicit SiftDetector(const Params& params)
        : params_(params)
    {}

    // 检测关键点并计算 128 维描述子（入口）。
    void DetectAndCompute(const cv::Mat& gray, std::vector<cv::KeyPoint>& keypoints, cv::Mat& descriptors) const;

private:
    Params params_;
};

} // namespace FeatureMatching

#endif // INC_FEATUREMATCHING_SIFTDETECTOR_H

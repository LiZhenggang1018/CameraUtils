#ifndef INC_FEATUREMATCHING_LOCALFEATURE_H
#define INC_FEATUREMATCHING_LOCALFEATURE_H

#include <vector>

#include "OpenCVCommon.h"

// 局部特征模块（SIFT / ORB / AKAZE）。
// 定位：读懂原理 + 调用 OpenCV，封装成统一语义的可复用接口。
// SURF 因本机 OpenCV 无 xfeatures2d contrib 模块，仅在学习笔记里讲述，不提供实现。
namespace FeatureMatching {

// 创建 SIFT 检测器（nfeatures 保留的最强特征数上限）。
inline cv::Ptr<cv::SIFT> CreateSift(int nfeatures)
{
    return cv::SIFT::create(nfeatures);
}

// 创建 ORB 检测器（nfeatures 保留的最强特征数上限）。
inline cv::Ptr<cv::ORB> CreateOrb(int nfeatures)
{
    return cv::ORB::create(nfeatures);
}

// 创建 AKAZE 检测器（默认参数）。
inline cv::Ptr<cv::AKAZE> CreateAkaze()
{
    return cv::AKAZE::create();
}

// 统一检测 + 描述：空输入或空检测器安全返回空。
inline void DetectAndCompute(const cv::Ptr<cv::Feature2D>& detector, const cv::Mat& image,
                             std::vector<cv::KeyPoint>& keypoints, cv::Mat& descriptors)
{
    keypoints.clear();
    descriptors = cv::Mat();
    if (detector == nullptr || image.empty()) {
        return;
    }
    detector->detectAndCompute(image, cv::noArray(), keypoints, descriptors);
}

} // namespace FeatureMatching

#endif // INC_FEATUREMATCHING_LOCALFEATURE_H

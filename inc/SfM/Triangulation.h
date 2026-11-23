#ifndef INC_SFM_TRIANGULATION_H
#define INC_SFM_TRIANGULATION_H

#include <vector>

#include "OpenCVCommon.h"

// 三角化模块。
// 定位：读懂原理 + 调用 OpenCV。两视图重建 3D 点。
namespace SfM {

// 用两视图投影矩阵 P1/P2（3×4）三角化对应点，返回 4×N 齐次坐标（第 4 行是尺度）。
// 点不足 2 或投影矩阵非法时返回空。
inline cv::Mat TriangulatePoints(const cv::Mat& P1, const cv::Mat& P2, const std::vector<cv::Point2f>& pts1,
                                 const std::vector<cv::Point2f>& pts2)
{
    if (pts1.empty() || pts1.size() != pts2.size() || P1.empty() || P2.empty()) {
        return {};
    }
    if (P1.rows != 3 || P1.cols != 4 || P2.rows != 3 || P2.cols != 4) {
        return {};
    }

    cv::Mat pts4d;
    try {
        cv::triangulatePoints(P1, P2, pts1, pts2, pts4d);
    } catch (const cv::Exception&) {
        return {};
    }
    if (pts4d.empty()) {
        return {};
    }
    // 统一转 CV_64F（triangulatePoints 输出类型可能为 CV_32F）
    cv::Mat out;
    pts4d.convertTo(out, CV_64F);
    return out;
}

} // namespace SfM

#endif // INC_SFM_TRIANGULATION_H

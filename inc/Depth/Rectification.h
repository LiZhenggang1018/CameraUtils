#ifndef INC_DEPTH_RECTIFICATION_H
#define INC_DEPTH_RECTIFICATION_H

#include <vector>

#include "OpenCVCommon.h"

#include "SfM/Epipolar.h"

// 极线校正模块。
// 定位：读懂原理 + 调用 OpenCV。未标定极线校正（只需 F，无需内参）。
// 流程：ComputeFundamental 求 F → stereoRectifyUncalibrated 求校正单应 H1/H2。
namespace Depth {

// 未标定极线校正：输入两视图对应点，输出把两图扭到「对极线水平」的单应 H1/H2。
// 成功返回 true，特征不足/退化返回 false。
inline bool RectifyUncalibrated(const std::vector<cv::Point2f>& pts1, const std::vector<cv::Point2f>& pts2,
                                cv::Size imageSize, cv::Mat& H1, cv::Mat& H2)
{
    if (pts1.size() < 8 || pts1.size() != pts2.size() || imageSize.width <= 0 || imageSize.height <= 0) {
        return false;
    }

    const cv::Mat F = SfM::ComputeFundamental(pts1, pts2);
    if (F.empty()) {
        return false;
    }

    try {
        const bool ok = cv::stereoRectifyUncalibrated(pts1, pts2, F, imageSize, H1, H2, 3);
        if (!ok) {
            return false;
        }
    } catch (const cv::Exception&) {
        return false;
    }

    return !H1.empty() && !H2.empty();
}

} // namespace Depth

#endif // INC_DEPTH_RECTIFICATION_H

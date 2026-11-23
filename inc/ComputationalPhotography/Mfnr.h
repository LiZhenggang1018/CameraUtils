#ifndef INC_COMPUTATIONALPHOTOGRAPHY_MFNR_H
#define INC_COMPUTATIONALPHOTOGRAPHY_MFNR_H

#include <vector>

#include "OpenCVCommon.h"

// MFNR（多帧降噪）模块。
// 定位：读懂原理 + 调用 OpenCV。同曝光多帧对齐 + 时域平均降噪。
// 实现：时域平均（假设帧已对齐），噪声方差随帧数 N 降低为 1/N。strength 控制可选的空间滤波强度。
namespace ComputationalPhotography {

inline cv::Mat MfnrDenoise(const std::vector<cv::Mat>& frames, int strength)
{
    if (frames.empty()) {
        return {};
    }
    if (frames.size() == 1) {
        return frames[0].clone();
    }

    const cv::Size refSize = frames[0].size();
    for (const auto& f : frames) {
        if (f.empty() || f.size() != refSize) {
            return frames[0].clone();
        }
    }

    // 时域平均：累加求均值（浮点域避免溢出）
    cv::Mat acc(refSize, CV_32FC(frames[0].channels()), cv::Scalar(0));
    for (const auto& f : frames) {
        cv::Mat f32;
        f.convertTo(f32, CV_32FC(f.channels()));
        cv::add(acc, f32, acc);
    }
    acc /= static_cast<double>(frames.size());

    cv::Mat denoised;
    acc.convertTo(denoised, frames[0].type());

    // 可选的额外空间滤波（strength > 1 时做一次高斯平滑）
    if (strength > 1 && denoised.channels() == 1) {
        cv::GaussianBlur(denoised, denoised, cv::Size(3, 3), 0);
    } else if (strength > 1) {
        cv::GaussianBlur(denoised, denoised, cv::Size(3, 3), 0);
    }

    return denoised;
}

} // namespace ComputationalPhotography

#endif // INC_COMPUTATIONALPHOTOGRAPHY_MFNR_H

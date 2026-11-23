#ifndef INC_COMPUTATIONALPHOTOGRAPHY_MFNR_H
#define INC_COMPUTATIONALPHOTOGRAPHY_MFNR_H

#include <vector>

#include "OpenCVCommon.h"

#include "LivePhotoSmoothPlay/ImageRegistrator.h"

// MFNR（多帧降噪）模块。
// 定位：读懂原理 + 调用 OpenCV。同曝光多帧「对齐 + 时域平均」降噪。
// 流程：每帧对齐到参考帧（ImageRegistrator），对齐失败的帧用原帧，然后时域平均降噪。
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
    const int refChannels = frames[0].channels();
    const int refType = frames[0].type();
    for (const auto& f : frames) {
        if (f.empty() || f.size() != refSize || f.channels() != refChannels || f.type() != refType) {
            return frames[0].clone();
        }
    }

    // 对齐：每帧对齐到 frames[0]（配准失败用原帧）
    ImageRegistrator registrator;
    std::vector<cv::Mat> aligned;
    aligned.reserve(frames.size());
    aligned.push_back(frames[0]);
    for (std::size_t i = 1; i < frames.size(); ++i) {
        const auto result = registrator.RegisterImages(frames[0], frames[i]);
        if (result.registeredImage.empty()) {
            aligned.push_back(frames[i]);
        } else {
            aligned.push_back(result.registeredImage);
        }
    }

    // 时域平均：累加求均值（浮点域避免溢出）
    cv::Mat acc(refSize, CV_32FC(refChannels), cv::Scalar(0));
    for (const auto& f : aligned) {
        cv::Mat f32;
        f.convertTo(f32, CV_32FC(f.channels()));
        cv::add(acc, f32, acc);
    }
    acc /= static_cast<double>(aligned.size());

    cv::Mat denoised;
    acc.convertTo(denoised, frames[0].type());

    // 可选的额外空间平滑（strength > 1 时）
    if (strength > 1) {
        cv::GaussianBlur(denoised, denoised, cv::Size(3, 3), 0);
    }

    return denoised;
}

} // namespace ComputationalPhotography

#endif // INC_COMPUTATIONALPHOTOGRAPHY_MFNR_H

#ifndef FRAME_INTERPOLATOR_H
#define FRAME_INTERPOLATOR_H

#include "OpenCVCommon.h"

class FrameInterpolator {
public:
    struct Params {
        // 光流后端：0 = Farneback，1 = DIS（ULTRAFAST 预设，快 2~3 倍）
        int flowBackend = 1;

        // 前后向一致性阈值（像素）：越大越宽松
        double fbThreshold = 1.0;

        // 【优化 2】静态区域保护：位移 < threshold 的像素直接用线性混合
        bool   enableStaticProtect = true;
        double staticMagThreshold  = 1.5;

        // 【优化 4】遮挡区软化：confidence / occWeight 都做高斯模糊，
        //          避免硬边界导致的撕裂
        double confidenceBlurSigma = 3.0;
        double occlusionBlurSigma  = 2.0;
    };

    FrameInterpolator() = default;
    explicit FrameInterpolator(const Params& params) : m_params(params) {}

    [[nodiscard]] cv::Mat Interpolate(
        const cv::Mat& prev,
        const cv::Mat& curr,
        double t = 0.5) const;

    // 参数配置（可选）
    void SetFbThreshold(double v) { m_params.fbThreshold = v; }
    [[nodiscard]] double FbThreshold() const { return m_params.fbThreshold; }

    void SetFlowBackend(int b) { m_params.flowBackend = b; }
    [[nodiscard]] int FlowBackend() const { return m_params.flowBackend; }

    void SetStaticProtect(bool enable) { m_params.enableStaticProtect = enable; }
    [[nodiscard]] bool IsStaticProtectEnabled() const { return m_params.enableStaticProtect; }

private:
    void ComputeFlow(const cv::Mat& from, const cv::Mat& to, cv::Mat& flow) const;

    Params m_params;
};

#endif // FRAME_INTERPOLATOR_H
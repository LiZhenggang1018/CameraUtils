#ifndef FRAME_INTERPOLATOR_H
#define FRAME_INTERPOLATOR_H

#include "OpenCVCommon.h"

class FrameInterpolator {
public:
    struct Params {
        // 光流后端：0 = Farneback，1 = DIS（ULTRAFAST 预设）
        int    flowBackend         = 1;

        // 前后向一致性阈值（像素）
        double fbThreshold         = 1.0;

        // 静态区域保护
        bool   enableStaticProtect = true;
        double staticMagThreshold  = 1.5;

        // 遮挡区软化：confidence / occWeight 高斯模糊 sigma
        double confidenceBlurSigma = 3.0;
        double occlusionBlurSigma  = 2.0;
    };

    FrameInterpolator() = default;
    explicit FrameInterpolator(const Params& params) : m_params(params) {}

    // ---- 单步入口（内部调用下面两个方法） ----
    [[nodiscard]] cv::Mat Interpolate(
        const cv::Mat& prev,
        const cv::Mat& curr,
        double t = 0.5) const;

    // ---- 两步拆分：供多倍率插帧复用光流 ----
    // 计算双向光流（只依赖 prev / curr，与 t 无关）
    void ComputeFlows(const cv::Mat& prev,
                      const cv::Mat& curr,
                      cv::Mat& flowFwd,
                      cv::Mat& flowBwd) const;

    // 用已算好的光流融合出时刻 t 的中间帧
    [[nodiscard]] cv::Mat BlendFromFlows(
        const cv::Mat& prev,
        const cv::Mat& curr,
        const cv::Mat& flowFwd,
        const cv::Mat& flowBwd,
        double t) const;

    // ---- 参数访问 ----
    void SetFbThreshold(double v) { m_params.fbThreshold = v; }
    [[nodiscard]] double FbThreshold() const { return m_params.fbThreshold; }

    void SetFlowBackend(int b) { m_params.flowBackend = b; }
    [[nodiscard]] int FlowBackend() const { return m_params.flowBackend; }

    void SetStaticProtect(bool enable) { m_params.enableStaticProtect = enable; }
    [[nodiscard]] bool IsStaticProtectEnabled() const { return m_params.enableStaticProtect; }

    [[nodiscard]] const Params& GetParams() const { return m_params; }

private:
    void ComputeFlow(const cv::Mat& from, const cv::Mat& to, cv::Mat& flow) const;

    Params m_params;
};

#endif // FRAME_INTERPOLATOR_H
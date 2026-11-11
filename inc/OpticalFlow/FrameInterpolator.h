#ifndef FRAME_INTERPOLATOR_H
#define FRAME_INTERPOLATOR_H

#include "OpenCVCommon.h"

class FrameInterpolator {
public:
    struct Params {
        // ---- 光流后端 ----
        int    flowBackend         = 1;      // 0=Farneback, 1=DIS, 2=TV-L1（慢但边缘保持好）
        int    disPreset           = 0;      // 0=ULTRAFAST, 1=FAST, 2=MEDIUM（仅 flowBackend==1 生效）
        int    maxCoarseLevels     = 1;      // 大位移处理：外部金字塔层数（0=禁用）

        // ---- 前后向一致性 ----
        double fbThreshold         = 1.0;

        // ---- 静态区保护 ----
        bool   enableStaticProtect = true;
        double staticMagThreshold  = 1.5;

        // ---- 遮挡区软化 ----
        double confidenceBlurSigma = 3.0;
        double occlusionBlurSigma  = 2.0;

        // ---- 时域平滑 ----
        bool   enableTemporalSmooth  = true;
        double temporalAlpha         = 0.85;  // 新帧权重（0.85=信新85%、信旧15%）
        double temporalMagThreshold  = 5.0;   // 只对 |F| < 此值的区域做平滑
    };

    FrameInterpolator() = default;
    explicit FrameInterpolator(const Params& params) : m_params(params) {}

    [[nodiscard]] cv::Mat Interpolate(
        const cv::Mat& prev,
        const cv::Mat& curr,
        double t = 0.5) const;

    void ComputeFlows(const cv::Mat& prev,
                      const cv::Mat& curr,
                      cv::Mat& flowFwd,
                      cv::Mat& flowBwd) const;

    [[nodiscard]] cv::Mat BlendFromFlows(
        const cv::Mat& prev,
        const cv::Mat& curr,
        const cv::Mat& flowFwd,
        const cv::Mat& flowBwd,
        double t) const;

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

    // 时域平滑状态（mutable 允许在 const 函数里更新）
    mutable cv::Mat m_prevFlowFwd;
};

#endif // FRAME_INTERPOLATOR_H
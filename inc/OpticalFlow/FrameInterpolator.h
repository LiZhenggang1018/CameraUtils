#ifndef FRAME_INTERPOLATOR_H
#define FRAME_INTERPOLATOR_H

#include "OpenCVCommon.h"

class FrameInterpolator {
public:
    struct Params {
        // ============ 光流后端 ============
        int    flowBackend         = 1;      // 0=Farneback, 1=DIS, 2=TV-L1
        int    disPreset           = 0;      // 0=ULTRAFAST, 1=FAST, 2=MEDIUM
        int    maxCoarseLevels     = 1;      // DIS 大位移处理层数（flowDownscale<1.0 时自动禁用）

        // ============ 性能开关（新增）============
        double flowDownscale           = 1.0;    // 光流计算分辨率：1.0=全，0.5=半，0.25=1/4
        bool   enableBilateralPreprocess = true; // 光流前置双边滤波
        bool   enableFlowPostProcess     = true; // 光流后处理（guidedFilter）
        bool   enableLaplacianBlend      = true; // 拉普拉斯金字塔融合
        int    laplacianLevels           = 4;    // 金字塔层数：2/3/4

        // ============ 质量参数 ============
        double fbThreshold         = 1.0;
        bool   enableStaticProtect = true;
        double staticMagThreshold  = 1.5;
        double confidenceBlurSigma = 3.0;
        double occlusionBlurSigma  = 2.0;
        bool   enableTemporalSmooth  = true;
        double temporalAlpha         = 0.85;
        double temporalMagThreshold  = 5.0;
    };

    FrameInterpolator() = default;
    explicit FrameInterpolator(const Params& params) : m_params(params) {}

    [[nodiscard]] cv::Mat Interpolate(const cv::Mat& prev, const cv::Mat& curr, double t = 0.5) const;
    void ComputeFlows(const cv::Mat& prev, const cv::Mat& curr, cv::Mat& flowFwd, cv::Mat& flowBwd) const;
    [[nodiscard]] cv::Mat BlendFromFlows(const cv::Mat& prev, const cv::Mat& curr,
                                          const cv::Mat& flowFwd, const cv::Mat& flowBwd, double t) const;

    [[nodiscard]] const Params& GetParams() const { return m_params; }

private:
    // 内部：全分辨率光流计算（不含降分辨率包装）
    void ComputeFlowInternal(const cv::Mat& from, const cv::Mat& to, cv::Mat& flow) const;

    // 对外：带 flowDownscale 包装
    void ComputeFlow(const cv::Mat& from, const cv::Mat& to, cv::Mat& flow) const;

    Params m_params;
    mutable cv::Mat m_prevFlowFwd;
};

#endif // FRAME_INTERPOLATOR_H
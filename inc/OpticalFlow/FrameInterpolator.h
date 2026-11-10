#ifndef FRAME_INTERPOLATOR_H
#define FRAME_INTERPOLATOR_H

#include "OpenCVCommon.h"

class FrameInterpolator {
public:
    struct Params {
        // 前后向一致性阈值（像素）：越大越宽松，遮挡误判越少但伪影越多
        double fbThreshold = 1.0;

        // 光流后端：0 = Farneback，1 = DIS（DIS 更快，Farneback 更平滑）
        int flowBackend = 1;
    };

    FrameInterpolator() = default;

    explicit FrameInterpolator(const Params& params)
        : m_params(params) {}

    // prev/curr 支持 BGR 或 GRAY；t=0.5 表示正中间帧
    [[nodiscard]] cv::Mat Interpolate(
        const cv::Mat& prev,
        const cv::Mat& curr,
        double t = 0.5) const;

    void SetFbThreshold(double threshold) { m_params.fbThreshold = threshold; }
    [[nodiscard]] double FbThreshold() const { return m_params.fbThreshold; }

    void SetFlowBackend(int backend) { m_params.flowBackend = backend; }
    [[nodiscard]] int FlowBackend() const { return m_params.flowBackend; }

private:
    void ComputeFlow(const cv::Mat& from, const cv::Mat& to,
                     cv::Mat& flow) const;

    Params m_params;
};

#endif // FRAME_INTERPOLATOR_H
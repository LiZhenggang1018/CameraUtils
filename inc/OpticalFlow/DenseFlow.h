#ifndef DENSE_FLOW_H
#define DENSE_FLOW_H

#include "OpenCVCommon.h"

class DenseFlowFarneback {
public:
    struct Params {
        double m_pyrScale = 0.5;
        int m_levels = 3;
        int m_winsize = 15;
        int m_iterations = 3;
        int m_polyN = 5;
        double m_polySigma = 1.1;
        int m_flags = 0;
    };

    DenseFlowFarneback() = default;

    explicit DenseFlowFarneback(const Params& params)
        : m_params(params) {}

    [[nodiscard]] cv::Mat Compute(
        const cv::Mat& prevGray,
        const cv::Mat& gray) const {
        cv::Mat flow;

        cv::calcOpticalFlowFarneback(
            prevGray,
            gray,
            flow,
            m_params.m_pyrScale,
            m_params.m_levels,
            m_params.m_winsize,
            m_params.m_iterations,
            m_params.m_polyN,
            m_params.m_polySigma,
            m_params.m_flags);

        return flow;
    }

    void SetWinsize(int winsize) {
        m_params.m_winsize = winsize;
    }

    [[nodiscard]] int Winsize() const {
        return m_params.m_winsize;
    }

private:
    Params m_params;
};

#endif
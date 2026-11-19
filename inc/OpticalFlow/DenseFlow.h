#ifndef INC_OPTICALFLOW_DENSEFLOW_H
#define INC_OPTICALFLOW_DENSEFLOW_H

#include "OpenCVCommon.h"

class DenseFlowFarneback {
public:
    struct Params {
        double pyrScale = 0.5;
        int levels = 3;
        int winsize = 15;
        int iterations = 3;
        int polyN = 5;
        double polySigma = 1.1;
        int flags = 0;
    };

    DenseFlowFarneback() = default;

    explicit DenseFlowFarneback(const Params& params)
        : params_(params)
    {}

    [[nodiscard]] cv::Mat Compute(const cv::Mat& prevGray, const cv::Mat& gray) const
    {
        cv::Mat flow;

        cv::calcOpticalFlowFarneback(prevGray, gray, flow, params_.pyrScale, params_.levels, params_.winsize,
                                     params_.iterations, params_.polyN, params_.polySigma, params_.flags);

        return flow;
    }

    void SetWinsize(int winsize) { params_.winsize = winsize; }

    [[nodiscard]] int Winsize() const { return params_.winsize; }

private:
    Params params_;
};

#endif
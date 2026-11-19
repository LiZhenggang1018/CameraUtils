#ifndef INC_OPTICALFLOW_DENSEFLOW_H
#define INC_OPTICALFLOW_DENSEFLOW_H

#include "OpenCVCommon.h"

class DenseFlowFarneback {
public:
    struct Params {
        double pyrScale_ = 0.5;
        int levels_ = 3;
        int winsize_ = 15;
        int iterations_ = 3;
        int polyN_ = 5;
        double polySigma_ = 1.1;
        int flags_ = 0;
    };

    DenseFlowFarneback() = default;

    explicit DenseFlowFarneback(const Params& params)
        : params_(params)
    {}

    [[nodiscard]] cv::Mat Compute(const cv::Mat& prevGray, const cv::Mat& gray) const
    {
        cv::Mat flow;

        cv::calcOpticalFlowFarneback(prevGray, gray, flow, params_.pyrScale_, params_.levels_, params_.winsize_,
                                     params_.iterations_, params_.polyN_, params_.polySigma_, params_.flags_);

        return flow;
    }

    void SetWinsize(int winsize) { params_.winsize_ = winsize; }

    [[nodiscard]] int Winsize() const { return params_.winsize_; }

private:
    Params params_;
};

#endif
#ifndef INC_OPTICALFLOW_DENSEFLOWDEEPFLOW_H
#define INC_OPTICALFLOW_DENSEFLOWDEEPFLOW_H

#include "OpenCVCommon.h"

class DenseFlowDeepFlow {
public:
    DenseFlowDeepFlow() { m_deepFlow = cv::optflow::createOptFlow_DeepFlow(); }

    [[nodiscard]] cv::Mat Compute(const cv::Mat& prevGray, const cv::Mat& gray) const
    {
        cv::Mat flow;
        m_deepFlow->calc(prevGray, gray, flow);
        return flow;
    }

private:
    cv::Ptr<cv::DenseOpticalFlow> m_deepFlow;
};

#endif
#include "BiDirectionalLkFilter.h"

void BiDirectionalLkFilter::Filter(
    const cv::Mat& prevGray,
    const cv::Mat& gray,
    const std::vector<cv::Point2f>& prevPts,
    std::vector<cv::Point2f>& outPrevPts,
    std::vector<cv::Point2f>& outCurrPts,
    std::vector<float>& outFbErrors) const {

    outPrevPts.clear();
    outCurrPts.clear();
    outFbErrors.clear();

    if (prevPts.empty()) {
        return;
    }

    // 1) 前向 LK：prev -> curr
    std::vector<cv::Point2f> forwardPts;
    std::vector<uchar> forwardStatus;
    std::vector<float> forwardErr;

    cv::calcOpticalFlowPyrLK(
        prevGray,
        gray,
        prevPts,
        forwardPts,
        forwardStatus,
        forwardErr,
        m_params.winSize,
        m_params.maxLevel,
        m_params.criteria);

    // 2) 收集前向成功的点，作为反向跟踪的输入
    std::vector<cv::Point2f> validPrev;
    std::vector<cv::Point2f> validForward;
    validPrev.reserve(prevPts.size());
    validForward.reserve(prevPts.size());

    for (std::size_t i = 0; i < forwardStatus.size(); ++i) {
        if (forwardStatus[i] != 0) {
            validPrev.push_back(prevPts[i]);
            validForward.push_back(forwardPts[i]);
        }
    }

    if (validForward.empty()) {
        return;
    }

    // 3) 反向 LK：curr -> prev
    std::vector<cv::Point2f> backwardPts;
    std::vector<uchar> backwardStatus;
    std::vector<float> backwardErr;

    cv::calcOpticalFlowPyrLK(
        gray,
        prevGray,
        validForward,
        backwardPts,
        backwardStatus,
        backwardErr,
        m_params.winSize,
        m_params.maxLevel,
        m_params.criteria);

    // 4) 前后向一致性检验：|prevPt - backward(forward(prevPt))|
    for (std::size_t i = 0; i < backwardStatus.size(); ++i) {
        if (backwardStatus[i] == 0) {
            continue;
        }

        const float fbError = static_cast<float>(
            cv::norm(validPrev[i] - backwardPts[i]));

        if (fbError < static_cast<float>(m_params.fbThreshold)) {
            outPrevPts.push_back(validPrev[i]);
            outCurrPts.push_back(validForward[i]);
            outFbErrors.push_back(fbError);
        }
    }
}
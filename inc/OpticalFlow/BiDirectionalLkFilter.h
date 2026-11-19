#ifndef INC_OPTICALFLOW_BIDIRECTIONALLKFILTER_H
#define INC_OPTICALFLOW_BIDIRECTIONALLKFILTER_H

#include <vector>
#include "OpenCVCommon.h"

class BiDirectionalLkFilter {
public:
    struct Params {
        cv::Size winSize = cv::Size(21, 21);
        int maxLevel = 3;
        cv::TermCriteria criteria = cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 30, 0.01);
        double fbThreshold = 1.5;
    };

    BiDirectionalLkFilter() = default;

    explicit BiDirectionalLkFilter(const Params& params)
        : params_(params)
    {}

    void Filter(const cv::Mat& prevGray, const cv::Mat& gray, const std::vector<cv::Point2f>& prevPts,
                std::vector<cv::Point2f>& outPrevPts, std::vector<cv::Point2f>& outCurrPts,
                std::vector<float>& outFbErrors) const;

    void SetFbThreshold(double threshold) { params_.fbThreshold = threshold; }

    [[nodiscard]] double FbThreshold() const { return params_.fbThreshold; }

    void SetWinSize(const cv::Size& winSize) { params_.winSize = winSize; }

    [[nodiscard]] cv::Size WinSize() const { return params_.winSize; }

    void SetMaxLevel(int level) { params_.maxLevel = level; }

    [[nodiscard]] int MaxLevel() const { return params_.maxLevel; }

    void SetCriteria(const cv::TermCriteria& criteria) { params_.criteria = criteria; }

    [[nodiscard]] cv::TermCriteria Criteria() const { return params_.criteria; }

private:
    Params params_;
};

#endif
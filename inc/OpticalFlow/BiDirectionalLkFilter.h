#ifndef BI_DIRECTIONAL_LK_FILTER_H
#define BI_DIRECTIONAL_LK_FILTER_H

#include "OpenCVCommon.h"
#include <vector>

class BiDirectionalLkFilter {
public:
    struct Params {
        cv::Size winSize = cv::Size(21, 21);
        int maxLevel = 3;
        cv::TermCriteria criteria = cv::TermCriteria(
            cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 30, 0.01);
        double fbThreshold = 1.5;
    };

    BiDirectionalLkFilter() = default;

    explicit BiDirectionalLkFilter(const Params& params)
        : m_params(params) {}

    void Filter(
        const cv::Mat& prevGray,
        const cv::Mat& gray,
        const std::vector<cv::Point2f>& prevPts,
        std::vector<cv::Point2f>& outPrevPts,
        std::vector<cv::Point2f>& outCurrPts,
        std::vector<float>& outFbErrors) const;

    void SetFbThreshold(double threshold) {
        m_params.fbThreshold = threshold;
    }

    [[nodiscard]] double FbThreshold() const {
        return m_params.fbThreshold;
    }

    void SetWinSize(const cv::Size& winSize) {
        m_params.winSize = winSize;
    }

    [[nodiscard]] cv::Size WinSize() const {
        return m_params.winSize;
    }

    void SetMaxLevel(int level) {
        m_params.maxLevel = level;
    }

    [[nodiscard]] int MaxLevel() const {
        return m_params.maxLevel;
    }

    void SetCriteria(const cv::TermCriteria& criteria) {
        m_params.criteria = criteria;
    }

    [[nodiscard]] cv::TermCriteria Criteria() const {
        return m_params.criteria;
    }

private:
    Params m_params;
};

#endif
#ifndef INC_OPTICALFLOW_LKTRACKER_H_
#define INC_OPTICALFLOW_LKTRACKER_H_

#include "OpenCVCommon.h"
#include <vector>

class LkTracker {
public:
    struct Params {
        int m_maxCorners = 500;
        double m_qualityLevel = 0.01;
        double m_minDistance = 10.0;
        int m_blockSize = 3;
        bool m_useHarrisDetector = false;
        double m_k = 0.04;

        cv::Size m_winSize = cv::Size(21, 21);
        int m_maxLevel = 3;
        cv::TermCriteria m_criteria = cv::TermCriteria(
            cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 30, 0.01);

        double m_replenishRatio = 0.5;
    };

    explicit LkTracker(const Params& params = Params())
        : m_params(params) {}

    void Reset();

    void Detect(const cv::Mat& gray);

    void Track(const cv::Mat& prevGray, const cv::Mat& gray);

    void AddPoint(const cv::Point2f& point);

    bool RemovePoint(const cv::Point2f& point, double radius = 8.0);

    [[nodiscard]] const std::vector<cv::Point2f>& Points() const {
        return m_currPts;
    }

    [[nodiscard]] const std::vector<cv::Point2f>& PrevPoints() const {
        return m_prevPts;
    }

    [[nodiscard]] int TrackedCount() const {
        return static_cast<int>(m_currPts.size());
    }

    void SetMaxLevel(int level) {
        m_params.m_maxLevel = level;
    }

    [[nodiscard]] int MaxLevel() const {
        return m_params.m_maxLevel;
    }

private:
    void Replenish(const cv::Mat& gray);

    Params m_params;
    std::vector<cv::Point2f> m_prevPts;
    std::vector<cv::Point2f> m_currPts;
};

#endif // INC_OPTICALFLOW_LKTRACKER_H_
#ifndef INC_OPTICALFLOW_LKTRACKER_H_
#define INC_OPTICALFLOW_LKTRACKER_H_

#include "OpenCVCommon.h"
#include <vector>

class LkTracker {
public:
    struct Params {
        int maxCorners = 500;
        double qualityLevel = 0.01;
        double minDistance = 10.0;
        int blockSize = 3;
        bool useHarrisDetector = false;
        double k = 0.04;

        cv::Size winSize = cv::Size(21, 21);
        int maxLevel = 3;
        cv::TermCriteria criteria = cv::TermCriteria(
            cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 30, 0.01);

        double replenishRatio = 0.5;

        // 双向 LK 参数
        bool enableFbFilter = true;
        double fbThreshold = 1.5;
    };

    explicit LkTracker(const Params& params = Params());

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

    void SetMaxLevel(int level);

    [[nodiscard]] int MaxLevel() const {
        return m_params.maxLevel;
    }

    void SetFbThreshold(double threshold);

    [[nodiscard]] double FbThreshold() const {
        return m_params.fbThreshold;
    }

    void SetEnableFbFilter(bool enable) {
        m_params.enableFbFilter = enable;
    }

    [[nodiscard]] bool IsFbFilterEnabled() const {
        return m_params.enableFbFilter;
    }

    [[nodiscard]] double AverageFbError() const;

private:
    static BiDirectionalLkFilter::Params MakeFbParams(const Params& params);

    void Replenish(const cv::Mat& gray);

    Params m_params;
    std::vector<cv::Point2f> m_prevPts;
    std::vector<cv::Point2f> m_currPts;
    std::vector<float> m_lastFbErrors;
    BiDirectionalLkFilter m_fbFilter;
};

#endif // INC_OPTICALFLOW_LKTRACKER_H_
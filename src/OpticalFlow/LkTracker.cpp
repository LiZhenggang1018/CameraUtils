#include "LkTracker.h"

#include <utility>

LkTracker::LkTracker(const Params& params)
    : m_params(params),
      m_fbFilter(MakeFbParams(params)) {}

BiDirectionalLkFilter::Params LkTracker::MakeFbParams(const Params& params) {
    BiDirectionalLkFilter::Params fbParams;
    fbParams.winSize = params.winSize;
    fbParams.maxLevel = params.maxLevel;
    fbParams.criteria = params.criteria;
    fbParams.fbThreshold = params.fbThreshold;
    return fbParams;
}

void LkTracker::Reset() {
    m_prevPts.clear();
    m_currPts.clear();
    m_lastFbErrors.clear();
}

void LkTracker::Detect(const cv::Mat& gray) {
    m_currPts.clear();

    cv::goodFeaturesToTrack(
        gray,
        m_currPts,
        m_params.maxCorners,
        m_params.qualityLevel,
        m_params.minDistance,
        cv::noArray(),
        m_params.blockSize,
        m_params.useHarrisDetector,
        m_params.k);

    m_prevPts = m_currPts;
}

void LkTracker::Track(const cv::Mat& prevGray, const cv::Mat& gray) {
    if (m_currPts.empty()) {
        Detect(gray);
        return;
    }

    std::vector<cv::Point2f> goodPrev;
    std::vector<cv::Point2f> goodCurr;

    if (m_params.enableFbFilter) {
        m_lastFbErrors.clear();
        m_fbFilter.Filter(prevGray, gray, m_currPts,
                          goodPrev, goodCurr, m_lastFbErrors);
    } else {
        std::vector<cv::Point2f> nextPts;
        std::vector<uchar> status;
        std::vector<float> err;

        cv::calcOpticalFlowPyrLK(
            prevGray,
            gray,
            m_currPts,
            nextPts,
            status,
            err,
            m_params.winSize,
            m_params.maxLevel,
            m_params.criteria);

        goodPrev.reserve(m_currPts.size());
        goodCurr.reserve(m_currPts.size());
        for (std::size_t i = 0; i < m_currPts.size(); ++i) {
            if (status[i] != 0) {
                goodPrev.push_back(m_currPts[i]);
                goodCurr.push_back(nextPts[i]);
            }
        }

        m_lastFbErrors.clear();
    }

    m_prevPts = std::move(goodPrev);
    m_currPts = std::move(goodCurr);

    if (m_currPts.size() <
        static_cast<std::size_t>(
            static_cast<double>(m_params.maxCorners) *
            m_params.replenishRatio)) {
        Replenish(gray);
    }
}

void LkTracker::Replenish(const cv::Mat& gray) {
    std::vector<cv::Point2f> candidates;
    cv::goodFeaturesToTrack(
        gray,
        candidates,
        m_params.maxCorners,
        m_params.qualityLevel,
        m_params.minDistance,
        cv::noArray(),
        m_params.blockSize,
        m_params.useHarrisDetector,
        m_params.k);

    for (const auto& point : candidates) {
        bool tooClose = false;
        for (const auto& trackedPoint : m_currPts) {
            if (cv::norm(point - trackedPoint) < m_params.minDistance) {
                tooClose = true;
                break;
            }
        }

        if (!tooClose) {
            m_currPts.push_back(point);
            m_prevPts.push_back(point);
        }
    }
}

void LkTracker::AddPoint(const cv::Point2f& point) {
    m_currPts.push_back(point);
    m_prevPts.push_back(point);
}

bool LkTracker::RemovePoint(const cv::Point2f& point, double radius) {
    bool removed = false;

    for (std::size_t i = 0; i < m_currPts.size();) {
        if (cv::norm(m_currPts[i] - point) <= radius) {
            m_currPts.erase(m_currPts.begin() +
                            static_cast<std::ptrdiff_t>(i));
            m_prevPts.erase(m_prevPts.begin() +
                            static_cast<std::ptrdiff_t>(i));
            removed = true;
        } else {
            ++i;
        }
    }

    return removed;
}

void LkTracker::SetMaxLevel(int level) {
    m_params.maxLevel = level;
    m_fbFilter.SetMaxLevel(level);
}

void LkTracker::SetFbThreshold(double threshold) {
    m_params.fbThreshold = threshold;
    m_fbFilter.SetFbThreshold(threshold);
}

double LkTracker::AverageFbError() const {
    if (m_lastFbErrors.empty()) {
        return 0.0;
    }

    double sum = 0.0;
    for (const float error : m_lastFbErrors) {
        sum += static_cast<double>(error);
    }

    return sum / static_cast<double>(m_lastFbErrors.size());
}
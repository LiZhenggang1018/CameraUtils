#include "LkTracker.h"

#include <utility>

void LkTracker::Reset() {
    m_prevPts.clear();
    m_currPts.clear();
}

void LkTracker::Detect(const cv::Mat& gray) {
    m_currPts.clear();

    cv::goodFeaturesToTrack(
        gray,
        m_currPts,
        m_params.m_maxCorners,
        m_params.m_qualityLevel,
        m_params.m_minDistance,
        cv::noArray(),
        m_params.m_blockSize,
        m_params.m_useHarrisDetector,
        m_params.m_k);

    m_prevPts = m_currPts;
}

void LkTracker::Track(const cv::Mat& prevGray, const cv::Mat& gray) {
    if (m_currPts.empty()) {
        Detect(gray);
        return;
    }

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
        m_params.m_winSize,
        m_params.m_maxLevel,
        m_params.m_criteria);

    std::vector<cv::Point2f> goodPrev;
    std::vector<cv::Point2f> goodCurr;
    goodPrev.reserve(m_currPts.size());
    goodCurr.reserve(m_currPts.size());

    for (std::size_t i = 0; i < m_currPts.size(); ++i) {
        if (status[i] != 0) {
            goodPrev.push_back(m_currPts[i]);
            goodCurr.push_back(nextPts[i]);
        }
    }

    m_prevPts = std::move(goodPrev);
    m_currPts = std::move(goodCurr);

    if (m_currPts.size() <
        static_cast<std::size_t>(
            static_cast<double>(m_params.m_maxCorners) *
            m_params.m_replenishRatio)) {
        Replenish(gray);
    }
}

void LkTracker::Replenish(const cv::Mat& gray) {
    std::vector<cv::Point2f> candidates;
    cv::goodFeaturesToTrack(
        gray,
        candidates,
        m_params.m_maxCorners,
        m_params.m_qualityLevel,
        m_params.m_minDistance,
        cv::noArray(),
        m_params.m_blockSize,
        m_params.m_useHarrisDetector,
        m_params.m_k);

    for (const auto& point : candidates) {
        bool tooClose = false;
        for (const auto& trackedPoint : m_currPts) {
            if (cv::norm(point - trackedPoint) < m_params.m_minDistance) {
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
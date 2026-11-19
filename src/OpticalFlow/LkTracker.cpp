#include "LkTracker.h"

#include <utility>

LkTracker::LkTracker(const Params& params)
    : params_(params),
      fbFilter_(MakeFbParams(params))
{}

BiDirectionalLkFilter::Params LkTracker::MakeFbParams(const Params& params)
{
    BiDirectionalLkFilter::Params fbParams;
    fbParams.winSize = params.winSize;
    fbParams.maxLevel = params.maxLevel;
    fbParams.criteria = params.criteria;
    fbParams.fbThreshold = params.fbThreshold;
    return fbParams;
}

void LkTracker::Reset()
{
    prevPts_.clear();
    currPts_.clear();
    lastFbErrors_.clear();
}

void LkTracker::Detect(const cv::Mat& gray)
{
    currPts_.clear();

    cv::goodFeaturesToTrack(gray, currPts_, params_.maxCorners, params_.qualityLevel, params_.minDistance,
                            cv::noArray(), params_.blockSize, params_.useHarrisDetector, params_.k);

    prevPts_ = currPts_;
}

void LkTracker::Track(const cv::Mat& prevGray, const cv::Mat& gray)
{
    if (currPts_.empty()) {
        Detect(gray);
        return;
    }

    std::vector<cv::Point2f> goodPrev;
    std::vector<cv::Point2f> goodCurr;

    if (params_.enableFbFilter) {
        lastFbErrors_.clear();
        fbFilter_.Filter(prevGray, gray, currPts_, goodPrev, goodCurr, lastFbErrors_);
    } else {
        std::vector<cv::Point2f> nextPts;
        std::vector<uchar> status;
        std::vector<float> err;

        cv::calcOpticalFlowPyrLK(prevGray, gray, currPts_, nextPts, status, err, params_.winSize, params_.maxLevel,
                                 params_.criteria);

        goodPrev.reserve(currPts_.size());
        goodCurr.reserve(currPts_.size());
        for (std::size_t i = 0; i < currPts_.size(); ++i) {
            if (status[i] != 0) {
                goodPrev.push_back(currPts_[i]);
                goodCurr.push_back(nextPts[i]);
            }
        }

        lastFbErrors_.clear();
    }

    prevPts_ = std::move(goodPrev);
    currPts_ = std::move(goodCurr);

    if (currPts_.size() < static_cast<std::size_t>(static_cast<double>(params_.maxCorners) * params_.replenishRatio)) {
        Replenish(gray);
    }
}

void LkTracker::Replenish(const cv::Mat& gray)
{
    std::vector<cv::Point2f> candidates;
    cv::goodFeaturesToTrack(gray, candidates, params_.maxCorners, params_.qualityLevel, params_.minDistance,
                            cv::noArray(), params_.blockSize, params_.useHarrisDetector, params_.k);

    for (const auto& point : candidates) {
        bool tooClose = false;
        for (const auto& trackedPoint : currPts_) {
            if (cv::norm(point - trackedPoint) < params_.minDistance) {
                tooClose = true;
                break;
            }
        }

        if (!tooClose) {
            currPts_.push_back(point);
            prevPts_.push_back(point);
        }
    }
}

void LkTracker::AddPoint(const cv::Point2f& point)
{
    currPts_.push_back(point);
    prevPts_.push_back(point);
}

bool LkTracker::RemovePoint(const cv::Point2f& point, double radius)
{
    bool removed = false;

    for (std::size_t i = 0; i < currPts_.size();) {
        if (cv::norm(currPts_[i] - point) <= radius) {
            currPts_.erase(currPts_.begin() + static_cast<std::ptrdiff_t>(i));
            prevPts_.erase(prevPts_.begin() + static_cast<std::ptrdiff_t>(i));
            removed = true;
        } else {
            ++i;
        }
    }

    return removed;
}

void LkTracker::SetMaxLevel(int level)
{
    params_.maxLevel = level;
    fbFilter_.SetMaxLevel(level);
}

void LkTracker::SetFbThreshold(double threshold)
{
    params_.fbThreshold = threshold;
    fbFilter_.SetFbThreshold(threshold);
}

double LkTracker::AverageFbError() const
{
    if (lastFbErrors_.empty()) {
        return 0.0;
    }

    double sum = 0.0;
    for (const float error : lastFbErrors_) {
        sum += static_cast<double>(error);
    }

    return sum / static_cast<double>(lastFbErrors_.size());
}
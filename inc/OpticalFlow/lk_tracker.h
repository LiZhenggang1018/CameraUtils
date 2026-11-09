#ifndef INC_OPTICALFLOW_LK_TRACKER_H_
#define INC_OPTICALFLOW_LK_TRACKER_H_

#include "OpenCVCommon.h"
#include <vector>

class LKTracker {
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
    };

    explicit LKTracker(const Params& params = Params()) : params_(params) {}

    void detect(const cv::Mat& gray);
    void track(const cv::Mat& prevGray, const cv::Mat& gray);

    const std::vector<cv::Point2f>& points() const { return currPts_; }
    const std::vector<cv::Point2f>& prevPoints() const { return prevPts_; }
    int trackedCount() const { return static_cast<int>(currPts_.size()); }

private:
    Params params_;
    std::vector<cv::Point2f> prevPts_;
    std::vector<cv::Point2f> currPts_;
};

#endif // INC_OPTICALFLOW_LK_TRACKER_H_

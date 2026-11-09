#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#include "OpenCVCommon.h"
#include <vector>

void DrawLkTrails(
    cv::Mat& image,
    const std::vector<cv::Point2f>& prevPts,
    const std::vector<cv::Point2f>& currPts,
    const cv::Scalar& color = cv::Scalar(0, 255, 0));

void DrawDenseFlowArrows(
    cv::Mat& image,
    const cv::Mat& flow,
    int step = 16,
    double scale = 1.0,
    const cv::Scalar& color = cv::Scalar(0, 255, 0));

#endif
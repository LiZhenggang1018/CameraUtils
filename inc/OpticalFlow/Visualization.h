#ifndef INC_OPTICALFLOW_VISUALIZATION_H
#define INC_OPTICALFLOW_VISUALIZATION_H

#include <vector>
#include "OpenCVCommon.h"

void DrawLkTrails(cv::Mat& image, const std::vector<cv::Point2f>& prevPts, const std::vector<cv::Point2f>& currPts,
                  const cv::Scalar& color = cv::Scalar(0, 255, 0));

void DrawDenseFlowArrows(cv::Mat& image, const cv::Mat& flow, int step = 16, double scale = 1.0,
                         const cv::Scalar& color = cv::Scalar(0, 255, 0));

#endif
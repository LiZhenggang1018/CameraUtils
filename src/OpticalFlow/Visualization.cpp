#include "Visualization.h"

void DrawLkTrails(
    cv::Mat& image,
    const std::vector<cv::Point2f>& prevPts,
    const std::vector<cv::Point2f>& currPts,
    const cv::Scalar& color) {
    for (std::size_t i = 0; i < currPts.size(); ++i) {
        cv::line(image, prevPts[i], currPts[i], color, 1, cv::LINE_AA);
        cv::circle(image, currPts[i], 3, color, -1, cv::LINE_AA);
    }
}

void DrawDenseFlowArrows(
    cv::Mat& image,
    const cv::Mat& flow,
    int step,
    double scale,
    const cv::Scalar& color) {
    for (int y = 0; y < flow.rows; y += step) {
        for (int x = 0; x < flow.cols; x += step) {
            const cv::Point2f fxy = flow.at<cv::Point2f>(y, x);
            const cv::Point2f end(
                static_cast<float>(x + fxy.x * scale),
                static_cast<float>(y + fxy.y * scale));

            cv::arrowedLine(
                image,
                cv::Point(x, y),
                cv::Point(cvRound(end.x), cvRound(end.y)),
                color,
                1,
                cv::LINE_AA,
                0,
                0.2);
        }
    }
}
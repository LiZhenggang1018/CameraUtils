#include "MiddleburyColor.h"

#include <vector>

cv::Mat FlowToColor(const cv::Mat& flow) {
    CV_Assert(flow.type() == CV_32FC2);

    cv::Mat flowParts[2];
    cv::split(flow, flowParts);

    cv::Mat magnitude;
    cv::Mat angle;
    cv::cartToPolar(
        flowParts[0],
        flowParts[1],
        magnitude,
        angle,
        true);

    double magMax = 0.0;
    cv::minMaxLoc(magnitude, nullptr, &magMax);

    if (magMax > 1e-6) {
        magnitude.convertTo(magnitude, CV_32F, 1.0 / magMax);
    } else {
        magnitude.setTo(0.0F);
    }

    std::vector<cv::Mat> hsv(3);
    hsv[0] = angle * 0.5F;
    hsv[1] = cv::Mat::ones(angle.size(), CV_32F) * 255.0F;
    hsv[2] = magnitude * 255.0F;

    cv::Mat hsvMerged;
    cv::merge(hsv, hsvMerged);
    hsvMerged.convertTo(hsvMerged, CV_8UC3);

    cv::Mat bgr;
    cv::cvtColor(hsvMerged, bgr, cv::COLOR_HSV2BGR);
    return bgr;
}
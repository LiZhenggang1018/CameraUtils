#include "FrameInterpolator.h"

namespace {

void BuildGrid(int h, int w, cv::Mat& gridX, cv::Mat& gridY) {
    gridX.create(h, w, CV_32F);
    gridY.create(h, w, CV_32F);
    for (int y = 0; y < h; ++y) {
        float* px = gridX.ptr<float>(y);
        float* py = gridY.ptr<float>(y);
        const float fy = static_cast<float>(y);
        for (int x = 0; x < w; ++x) {
            px[x] = static_cast<float>(x);
            py[x] = fy;
        }
    }
}

cv::Mat WarpToMid(const cv::Mat& from,
                  const cv::Mat& flow,
                  const cv::Mat& gridX,
                  const cv::Mat& gridY,
                  double scale) {
    std::vector<cv::Mat> parts(2);
    cv::split(flow, parts);

    cv::Mat mapX = gridX - scale * parts[0];
    cv::Mat mapY = gridY - scale * parts[1];

    cv::Mat out;
    cv::remap(from, out, mapX, mapY,
              cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    return out;
}

cv::Mat ComputeConfidence(const cv::Mat& flowFwd,
                          const cv::Mat& flowBwd,
                          const cv::Mat& gridX,
                          const cv::Mat& gridY,
                          double threshold) {
    std::vector<cv::Mat> fwdParts(2);
    cv::split(flowFwd, fwdParts);

    cv::Mat mapX = gridX + fwdParts[0];
    cv::Mat mapY = gridY + fwdParts[1];

    cv::Mat bwdAtFwd;
    cv::remap(flowBwd, bwdAtFwd, mapX, mapY,
              cv::INTER_LINEAR, cv::BORDER_REPLICATE);

    cv::Mat sumFlow;
    cv::add(flowFwd, bwdAtFwd, sumFlow);

    std::vector<cv::Mat> sumParts(2);
    cv::split(sumFlow, sumParts);
    cv::Mat err2 = sumParts[0].mul(sumParts[0]) + sumParts[1].mul(sumParts[1]);

    cv::Mat mask8;
    cv::compare(err2, cv::Scalar(threshold * threshold), mask8, cv::CMP_LT);

    cv::Mat confidence;
    mask8.convertTo(confidence, CV_32F, 1.0 / 255.0);
    cv::dilate(confidence, confidence,
               cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3)));
    return confidence;
}

cv::Mat ComputeStaticMask(const cv::Mat& flowFwd, double threshold) {
    std::vector<cv::Mat> parts(2);
    cv::split(flowFwd, parts);
    cv::Mat mag2 = parts[0].mul(parts[0]) + parts[1].mul(parts[1]);

    cv::Mat mask8;
    cv::compare(mag2, cv::Scalar(threshold * threshold), mask8, cv::CMP_LT);

    cv::Mat staticMask;
    mask8.convertTo(staticMask, CV_32F, 1.0 / 255.0);
    return staticMask;
}

cv::Mat ExpandWeightTo3Ch(const cv::Mat& weight) {
    cv::Mat ch[3] = { weight, weight, weight };
    cv::Mat w3;
    cv::merge(ch, 3, w3);
    return w3;
}

} // namespace

void FrameInterpolator::ComputeFlow(const cv::Mat& from,
                                    const cv::Mat& to,
                                    cv::Mat& flow) const {
    if (m_params.flowBackend == 1) {
        cv::Ptr<cv::DISOpticalFlow> dis =
            cv::DISOpticalFlow::create(cv::DISOpticalFlow::PRESET_ULTRAFAST);
        dis->calc(from, to, flow);
    } else {
        cv::calcOpticalFlowFarneback(
            from, to, flow,
            0.5, 3, 15, 3, 5, 1.1, 0);
    }
}

void FrameInterpolator::ComputeFlows(const cv::Mat& prev,
                                     const cv::Mat& curr,
                                     cv::Mat& flowFwd,
                                     cv::Mat& flowBwd) const {
    cv::Mat prevGray, currGray;
    if (prev.channels() == 3) {
        cv::cvtColor(prev, prevGray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(curr, currGray, cv::COLOR_BGR2GRAY);
    } else {
        prevGray = prev;
        currGray = curr;
    }

    ComputeFlow(prevGray, currGray, flowFwd);
    ComputeFlow(currGray, prevGray, flowBwd);
}

cv::Mat FrameInterpolator::BlendFromFlows(const cv::Mat& prev,
                                          const cv::Mat& curr,
                                          const cv::Mat& flowFwd,
                                          const cv::Mat& flowBwd,
                                          double t) const {
    CV_Assert(!prev.empty() && !curr.empty());
    CV_Assert(prev.size() == curr.size());
    CV_Assert(prev.type() == curr.type());
    CV_Assert(t > 0.0 && t < 1.0);
    CV_Assert(prev.size() == flowFwd.size());
    CV_Assert(prev.size() == flowBwd.size());

    const int h = prev.rows;
    const int w = prev.cols;

    cv::Mat gridX, gridY;
    BuildGrid(h, w, gridX, gridY);

    cv::Mat warpedPrev = WarpToMid(prev, flowFwd, gridX, gridY,  t);
    cv::Mat warpedCurr = WarpToMid(curr, flowBwd, gridX, gridY, -(1.0 - t));

    cv::Mat confidence = ComputeConfidence(
        flowFwd, flowBwd, gridX, gridY, m_params.fbThreshold);
    if (m_params.confidenceBlurSigma > 0.0) {
        cv::GaussianBlur(confidence, confidence,
                         cv::Size(0, 0), m_params.confidenceBlurSigma);
    }

    cv::Mat staticMask;
    if (m_params.enableStaticProtect) {
        staticMask = ComputeStaticMask(flowFwd, m_params.staticMagThreshold);
        cv::GaussianBlur(staticMask, staticMask, cv::Size(0, 0), 2.0);
    } else {
        staticMask = cv::Mat::zeros(h, w, CV_32F);
    }

    cv::Mat wpF, wcF, prevF, currF;
    warpedPrev.convertTo(wpF, CV_32FC3);
    warpedCurr.convertTo(wcF, CV_32FC3);
    prev.convertTo(prevF, CV_32FC3);
    curr.convertTo(currF, CV_32FC3);

    cv::Mat candidateWarped;
    cv::addWeighted(wpF, 1.0 - t, wcF, t, 0.0, candidateWarped);

    cv::Mat candidateLinear;
    cv::addWeighted(prevF, 1.0 - t, currF, t, 0.0, candidateLinear);

    cv::Mat magFwd;
    {
        std::vector<cv::Mat> parts(2);
        cv::split(flowFwd, parts);
        cv::magnitude(parts[0], parts[1], magFwd);
    }
    cv::Mat occWeight;
    cv::compare(magFwd, cv::Scalar(m_params.staticMagThreshold),
                occWeight, cv::CMP_GT);
    occWeight.convertTo(occWeight, CV_32F, 1.0 / 255.0);
    if (m_params.occlusionBlurSigma > 0.0) {
        cv::GaussianBlur(occWeight, occWeight,
                         cv::Size(0, 0), m_params.occlusionBlurSigma);
    }

    cv::Mat warpedAvg;
    cv::addWeighted(wpF, 0.5, wcF, 0.5, 0.0, warpedAvg);

    cv::Mat occW3 = ExpandWeightTo3Ch(occWeight);
    cv::Mat oneMinusOccW3;
    cv::subtract(cv::Scalar::all(1.0), occW3, oneMinusOccW3);

    cv::Mat candidateOccluded;
    cv::add(occW3.mul(warpedAvg),
            oneMinusOccW3.mul(candidateLinear),
            candidateOccluded);

    cv::Mat conf3   = ExpandWeightTo3Ch(confidence);
    cv::Mat static3 = ExpandWeightTo3Ch(staticMask);

    cv::Mat oneMinusConf3;
    cv::subtract(cv::Scalar::all(1.0), conf3, oneMinusConf3);
    cv::Mat oneMinusStatic3;
    cv::subtract(cv::Scalar::all(1.0), static3, oneMinusStatic3);

    cv::Mat nonStatic;
    cv::add(conf3.mul(candidateWarped),
            oneMinusConf3.mul(candidateOccluded),
            nonStatic);

    cv::Mat result32F;
    cv::add(static3.mul(candidateLinear),
            oneMinusStatic3.mul(nonStatic),
            result32F);

    cv::Mat result;
    result32F.convertTo(result, prev.type());
    return result;
}

cv::Mat FrameInterpolator::Interpolate(const cv::Mat& prev,
                                       const cv::Mat& curr,
                                       double t) const {
    cv::Mat fF, fB;
    ComputeFlows(prev, curr, fF, fB);
    return BlendFromFlows(prev, curr, fF, fB, t);
}
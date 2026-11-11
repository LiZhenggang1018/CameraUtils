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

cv::Mat WarpToMid(const cv::Mat& from, const cv::Mat& flow,
                  const cv::Mat& gridX, const cv::Mat& gridY, double scale) {
    std::vector<cv::Mat> parts(2);
    cv::split(flow, parts);
    cv::Mat mapX = gridX - scale * parts[0];
    cv::Mat mapY = gridY - scale * parts[1];
    cv::Mat out;
    cv::remap(from, out, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    return out;
}

cv::Mat ComputeConfidence(const cv::Mat& flowFwd, const cv::Mat& flowBwd,
                          const cv::Mat& gridX, const cv::Mat& gridY,
                          double threshold) {
    std::vector<cv::Mat> fwdParts(2);
    cv::split(flowFwd, fwdParts);
    cv::Mat mapX = gridX + fwdParts[0];
    cv::Mat mapY = gridY + fwdParts[1];
    cv::Mat bwdAtFwd;
    cv::remap(flowBwd, bwdAtFwd, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_REPLICATE);

    cv::Mat sumFlow;
    cv::add(flowFwd, bwdAtFwd, sumFlow);
    std::vector<cv::Mat> sumParts(2);
    cv::split(sumFlow, sumParts);
    cv::Mat err2 = sumParts[0].mul(sumParts[0]) + sumParts[1].mul(sumParts[1]);

    cv::Mat mask8;
    cv::compare(err2, cv::Scalar(threshold * threshold), mask8, cv::CMP_LT);
    cv::Mat confidence;
    mask8.convertTo(confidence, CV_32F, 1.0 / 255.0);
    // 注意：去掉 dilate，避免遮挡边缘误判为"一致"，减轻 ghost
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

cv::Mat ComputeFlowMagnitude(const cv::Mat& flow) {
    std::vector<cv::Mat> parts(2);
    cv::split(flow, parts);
    cv::Mat mag;
    cv::magnitude(parts[0], parts[1], mag);
    return mag;
}

} // namespace

// ============================================================
// 优化 4：大位移处理
//   粗分辨率 Farneback → 上采样作为 DIS 初值 → DIS 精修
// ============================================================
void FrameInterpolator::ComputeFlow(const cv::Mat& from,
                                    const cv::Mat& to,
                                    cv::Mat& flow) const {
    if (m_params.flowBackend == 0) {
        cv::calcOpticalFlowFarneback(from, to, flow, 0.5, 3, 15, 3, 5, 1.1, 0);
        return;
    }

    cv::Mat initialFlow;
    if (m_params.maxCoarseLevels > 0) {
        const double coarseScale = std::pow(0.5, m_params.maxCoarseLevels);
        cv::Mat fromCoarse, toCoarse;
        cv::resize(from, fromCoarse, cv::Size(), coarseScale, coarseScale, cv::INTER_AREA);
        cv::resize(to,   toCoarse,   cv::Size(), coarseScale, coarseScale, cv::INTER_AREA);

        cv::Mat flowCoarse;
        cv::calcOpticalFlowFarneback(fromCoarse, toCoarse, flowCoarse, 0.5, 3, 15, 3, 5, 1.1, 0);
        cv::resize(flowCoarse, initialFlow, from.size(), 0, 0, cv::INTER_LINEAR);
        initialFlow *= (1.0 / coarseScale);
    }

    int preset = cv::DISOpticalFlow::PRESET_ULTRAFAST;
    switch (m_params.disPreset) {
        case 1: preset = cv::DISOpticalFlow::PRESET_FAST;   break;
        case 2: preset = cv::DISOpticalFlow::PRESET_MEDIUM; break;
        default: preset = cv::DISOpticalFlow::PRESET_ULTRAFAST; break;
    }
    cv::Ptr<cv::DISOpticalFlow> dis = cv::DISOpticalFlow::create(preset);

    if (!initialFlow.empty()) {
        flow = initialFlow;
    } else {
        flow.release();
    }
    dis->calc(from, to, flow);
}

// ============================================================
// 优化 5：时域平滑 + 大招 2：bilateral 预处理
// ============================================================
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

    // 大招 2：双边滤波预处理，平滑纹理噪声
    cv::Mat prevSmooth, currSmooth;
    cv::bilateralFilter(prevGray, prevSmooth, 5, 25.0, 25.0);
    cv::bilateralFilter(currGray, currSmooth, 5, 25.0, 25.0);

    ComputeFlow(prevSmooth, currSmooth, flowFwd);
    ComputeFlow(currSmooth, prevSmooth, flowBwd);

    // 时域平滑
    if (m_params.enableTemporalSmooth
        && !m_prevFlowFwd.empty()
        && m_prevFlowFwd.size() == flowFwd.size()
        && m_prevFlowFwd.type() == flowFwd.type()) {

        cv::Mat mag = ComputeFlowMagnitude(flowFwd);

        cv::Mat smallMask8;
        cv::compare(mag, cv::Scalar(m_params.temporalMagThreshold), smallMask8, cv::CMP_LT);
        cv::Mat smallMask;
        smallMask8.convertTo(smallMask, CV_32F, 1.0 / 255.0);

        cv::Mat smoothFlow;
        cv::addWeighted(flowFwd, m_params.temporalAlpha,
                        m_prevFlowFwd, 1.0 - m_params.temporalAlpha,
                        0.0, smoothFlow);

        cv::Mat mask2;
        cv::Mat ch[2] = { smallMask, smallMask };
        cv::merge(ch, 2, mask2);
        cv::Mat invMask2;
        cv::subtract(cv::Scalar::all(1.0), mask2, invMask2);

        cv::Mat flowFwdSmoothed;
        cv::add(mask2.mul(smoothFlow), invMask2.mul(flowFwd), flowFwdSmoothed);
        flowFwd = flowFwdSmoothed;
    }

    m_prevFlowFwd = flowFwd.clone();
}

// ============================================================
// 融合：修复 1（动态加权） + 修复 2（combined²） + 修复 3（bilateral 后处理）
// ============================================================
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

    // ---------- 0. 坐标网格 + 反向映射 ----------
    cv::Mat gridX, gridY;
    BuildGrid(h, w, gridX, gridY);

    cv::Mat warpedPrev = WarpToMid(prev, flowFwd, gridX, gridY,  t);
    cv::Mat warpedCurr = WarpToMid(curr, flowBwd, gridX, gridY, -(1.0 - t));

    // ---------- 1. confidence（已去掉 dilate） ----------
    cv::Mat confidence = ComputeConfidence(
        flowFwd, flowBwd, gridX, gridY, m_params.fbThreshold);
    if (m_params.confidenceBlurSigma > 0.0) {
        cv::GaussianBlur(confidence, confidence,
                         cv::Size(0, 0), m_params.confidenceBlurSigma);
    }

    // ---------- 2. 静态区掩膜 ----------
    cv::Mat staticMask;
    if (m_params.enableStaticProtect) {
        staticMask = ComputeStaticMask(flowFwd, m_params.staticMagThreshold);
        cv::GaussianBlur(staticMask, staticMask, cv::Size(0, 0), 2.0);
    } else {
        staticMask = cv::Mat::zeros(h, w, CV_32F);
    }

    // ---------- 3. 转 float 三通道 ----------
    cv::Mat wpF, wcF, prevF, currF;
    warpedPrev.convertTo(wpF, CV_32FC3);
    warpedCurr.convertTo(wcF, CV_32FC3);
    prev.convertTo(prevF, CV_32FC3);
    curr.convertTo(currF, CV_32FC3);

    // ============================================================
    // 修复 1：非遮挡区动态加权
    //   t=0.5 时不再 50/50，改为"局部误差小的一侧"权重更大
    // ============================================================
    cv::Mat candidateWarped;
    if (std::abs(t - 0.5) < 1e-9) {
        // 局部平滑度对比：每个像素与 5x5 邻域均值的偏差越小越可信
        cv::Mat wpBlur, wcBlur;
        cv::blur(wpF, wpBlur, cv::Size(5, 5));
        cv::blur(wcF, wcBlur, cv::Size(5, 5));

        cv::Mat wpDev, wcDev;
        cv::absdiff(wpF, wpBlur, wpDev);
        cv::absdiff(wcF, wcBlur, wcDev);

        cv::Mat wpDevGray, wcDevGray;
        cv::cvtColor(wpDev, wpDevGray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(wcDev, wcDevGray, cv::COLOR_BGR2GRAY);

        cv::Mat wpBetter8;
        cv::compare(wpDevGray, wcDevGray, wpBetter8, cv::CMP_LT);
        cv::Mat wpBetter;
        wpBetter8.convertTo(wpBetter, CV_32F, 1.0 / 255.0);
        cv::GaussianBlur(wpBetter, wpBetter, cv::Size(0, 0), 3.0);

        cv::Mat wpBetter3 = ExpandWeightTo3Ch(wpBetter);
        cv::Mat wpWorse3;
        cv::subtract(cv::Scalar::all(1.0), wpBetter3, wpWorse3);

        cv::add(wpBetter3.mul(wpF), wpWorse3.mul(wcF), candidateWarped);
    } else {
        cv::addWeighted(wpF, 1.0 - t, wcF, t, 0.0, candidateWarped);
    }

    // ---------- 候选 B：线性混合（静态区） ----------
    cv::Mat candidateLinear;
    cv::addWeighted(prevF, 1.0 - t, currF, t, 0.0, candidateLinear);

    // ---------- 候选 C：遮挡区（修复 2） ----------
    cv::Mat magFwd = ComputeFlowMagnitude(flowFwd);
    cv::Mat occWeight8;
    cv::compare(magFwd, cv::Scalar(m_params.staticMagThreshold), occWeight8, cv::CMP_GT);
    cv::Mat occWeight;
    occWeight8.convertTo(occWeight, CV_32F, 1.0 / 255.0);
    if (m_params.occlusionBlurSigma > 0.0) {
        cv::GaussianBlur(occWeight, occWeight, cv::Size(0, 0), m_params.occlusionBlurSigma);
    }

    cv::Mat warpedAvg;
    cv::addWeighted(wpF, 0.5, wcF, 0.5, 0.0, warpedAvg);

    // 双向 warp 差异：差异大 = 真遮挡
    cv::Mat warpedDiff;
    cv::absdiff(wpF, wcF, warpedDiff);
    cv::Mat warpedDiffGray;
    cv::cvtColor(warpedDiff, warpedDiffGray, cv::COLOR_BGR2GRAY);
    cv::Mat diffMask8;
    cv::compare(warpedDiffGray, cv::Scalar(25.0), diffMask8, cv::CMP_GT);
    cv::Mat diffMask;
    diffMask8.convertTo(diffMask, CV_32F, 1.0 / 255.0);
    cv::GaussianBlur(diffMask, diffMask, cv::Size(0, 0), 1.5);

    // combinedMask = occWeight * diffMask
    cv::Mat combinedMask;
    cv::multiply(occWeight, diffMask, combinedMask);

    // ★ combinedW3 必须在这里定义，后面 combinedSq 要用
    cv::Mat combinedW3 = ExpandWeightTo3Ch(combinedMask);

    // combinedSq 让"中等遮挡"更偏向 candidateLinear（减少双影）
    cv::Mat combinedSq;
    cv::multiply(combinedW3, combinedW3, combinedSq);
    cv::Mat oneMinusCombinedSq;
    cv::subtract(cv::Scalar::all(1.0), combinedSq, oneMinusCombinedSq);

    cv::Mat candidateOccluded;
    cv::add(combinedSq.mul(candidateLinear),
            oneMinusCombinedSq.mul(warpedAvg),
            candidateOccluded);

    // ---------- 三区融合 ----------
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

    // ============================================================
    // 修复 3：遮挡区 bilateral 后处理（把双影变成轻微模糊）
    // ============================================================
    {
        cv::Mat confMask8;
        cv::compare(confidence, cv::Scalar(0.3), confMask8, cv::CMP_LT);
        cv::Mat confMask;
        confMask8.convertTo(confMask, CV_32F, 1.0 / 255.0);
        cv::Mat confMask3 = ExpandWeightTo3Ch(confMask);

        cv::Mat result8u;
        result32F.convertTo(result8u, CV_8UC3);
        cv::Mat smoothed8u;
        cv::bilateralFilter(result8u, smoothed8u, 5, 30.0, 30.0);
        cv::Mat smoothedF;
        smoothed8u.convertTo(smoothedF, CV_32FC3);

        cv::Mat oneMinusConfMask3;
        cv::subtract(cv::Scalar::all(1.0), confMask3, oneMinusConfMask3);
        cv::add(confMask3.mul(smoothedF),
                oneMinusConfMask3.mul(result32F),
                result32F);
    }

    // ---------- 转回 8UC3 ----------
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
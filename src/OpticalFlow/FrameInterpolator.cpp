#include "FrameInterpolator.h"

#include <opencv2/ximgproc.hpp>
#include <opencv2/optflow.hpp>

#include <cmath>
#include <cstdio>

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

cv::Mat ExpandWeightTo3Ch(const cv::Mat& weight) {
    cv::Mat ch[3] = { weight, weight, weight };
    cv::Mat w3;
    cv::merge(ch, 3, w3);
    return w3;
}

cv::Mat LaplacianBlend(const cv::Mat& prev8u, const cv::Mat& curr8u,
                       const cv::Mat& wpF, const cv::Mat& wcF,
                       const cv::Mat& confidence,
                       double t, int levels = 4) {
    cv::Mat prevF, currF;
    prev8u.convertTo(prevF, CV_32FC3);
    curr8u.convertTo(currF, CV_32FC3);

    // ★ 修复：buildPyramid 的 maxlevel = levels - 1，
    //   这样金字塔正好 levels 层（0..levels-1），和后面循环逻辑一致
    std::vector<cv::Mat> gp, gc, gwp, gwc;
    cv::buildPyramid(prevF, gp, levels - 1);
    cv::buildPyramid(currF, gc, levels - 1);
    cv::buildPyramid(wpF, gwp, levels - 1);
    cv::buildPyramid(wcF, gwc, levels - 1);

    // confidence 金字塔：每层用 resize 降采样到 gp[i] 尺寸
    std::vector<cv::Mat> lconf(levels);
    lconf[0] = confidence;
    for (int i = 1; i < levels; ++i) {
        cv::resize(lconf[i-1], lconf[i], gp[i].size(), 0, 0, cv::INTER_AREA);
    }

    // 拉普拉斯金字塔
    std::vector<cv::Mat> lp, lc, lwp, lwc;
    for (int i = 0; i < levels - 1; ++i) {
        cv::Mat upP, upC, upWP, upWC;
        cv::pyrUp(gp[i+1],  upP,  gp[i].size());
        cv::pyrUp(gc[i+1],  upC,  gc[i].size());
        cv::pyrUp(gwp[i+1], upWP, gwp[i].size());
        cv::pyrUp(gwc[i+1], upWC, gwc[i].size());
        cv::Mat lapP, lapC, lapWP, lapWC;
        cv::subtract(gp[i],  upP,  lapP);
        cv::subtract(gc[i],  upC,  lapC);
        cv::subtract(gwp[i], upWP, lapWP);
        cv::subtract(gwc[i], upWC, lapWC);
        lp.push_back(lapP);   lc.push_back(lapC);
        lwp.push_back(lapWP); lwc.push_back(lapWC);
    }
    lp.push_back(gp.back());   lc.push_back(gc.back());     // ← gp.back() = gp[3]
    lwp.push_back(gwp.back()); lwc.push_back(gwc.back());

    // 逐层融合
    std::vector<cv::Mat> lout;
    for (int i = 0; i < levels; ++i) {
        cv::Mat warpBlend, linearBlend;
        cv::addWeighted(lwp[i], 1.0 - t, lwc[i], t, 0.0, warpBlend);
        cv::addWeighted(lp[i],  1.0 - t, lc[i],  t, 0.0, linearBlend);

        cv::Mat blended;
        if (i == levels - 1) {
            blended = linearBlend;   // 最低频：linear，无 ghost
        } else if (i == 0) {
            blended = warpBlend;     // 最高频：warp，保清晰
        } else {
            cv::Mat conf3 = ExpandWeightTo3Ch(lconf[i]);
            cv::Mat oneMinusConf3;
            cv::subtract(cv::Scalar::all(1.0), conf3, oneMinusConf3);
            cv::add(conf3.mul(warpBlend),
                    oneMinusConf3.mul(linearBlend),
                    blended);
        }
        lout.push_back(blended);
    }

    // 重构（此时尺寸全对）
    cv::Mat result = lout[levels - 1];
    for (int i = levels - 2; i >= 0; --i) {
        cv::Mat up;
        cv::pyrUp(result, up, lout[i].size());
        cv::add(up, lout[i], result);
    }

    cv::Mat result8u;
    result.convertTo(result8u, CV_8UC3);
    return result8u;
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

cv::Mat ComputeFlowMagnitude(const cv::Mat& flow) {
    std::vector<cv::Mat> parts(2);
    cv::split(flow, parts);
    cv::Mat mag;
    cv::magnitude(parts[0], parts[1], mag);
    return mag;
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

    cv::Mat err2Local;
    cv::blur(err2, err2Local, cv::Size(5, 5));

    cv::Mat mag = ComputeFlowMagnitude(flowFwd);
    cv::Mat smallMag8;
    cv::compare(mag, cv::Scalar(2.0), smallMag8, cv::CMP_LT);
    cv::Mat smallMag;
    smallMag8.convertTo(smallMag, CV_32F, 1.0 / 255.0);
    cv::Mat largeMag;
    cv::subtract(cv::Scalar::all(1.0), smallMag, largeMag);

    // ★ 用 Scalar 构造阈值矩阵，避免 Mat::ones 的 MatExpr 在某些边界下报错
    const double thrSmall = 2.0 * threshold;
    cv::Mat thr2Small(err2.size(), CV_32F, cv::Scalar(thrSmall * thrSmall));
    cv::Mat thr2Large(err2.size(), CV_32F, cv::Scalar(threshold * threshold));

    cv::Mat maskSmall8, maskLarge8;
    cv::compare(err2Local, thr2Small, maskSmall8, cv::CMP_LT);
    cv::compare(err2Local, thr2Large, maskLarge8, cv::CMP_LT);
    cv::Mat maskSmall, maskLarge;
    maskSmall8.convertTo(maskSmall, CV_32F, 1.0 / 255.0);
    maskLarge8.convertTo(maskLarge, CV_32F, 1.0 / 255.0);

    cv::Mat confidence;
    cv::add(smallMag.mul(maskSmall), largeMag.mul(maskLarge), confidence);
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

void PostProcessFlow(const cv::Mat& flowIn,
                     const cv::Mat& guideGray,
                     cv::Mat& flowOut) {
    if (flowIn.empty()) {
        flowIn.copyTo(flowOut);
        return;
    }

    std::vector<cv::Mat> ch(2);
    cv::split(flowIn, ch);
    cv::medianBlur(ch[0], ch[0], 5);
    cv::medianBlur(ch[1], ch[1], 5);
    cv::Mat medianFlow;
    cv::merge(ch, medianFlow);

    std::vector<cv::Mat> ch2(2);
    cv::split(medianFlow, ch2);

    cv::Mat gf0, gf1;
    cv::ximgproc::guidedFilter(guideGray, ch2[0], gf0, 8, 100.0);
    cv::ximgproc::guidedFilter(guideGray, ch2[1], gf1, 8, 100.0);

    ch2[0] = gf0;
    ch2[1] = gf1;
    cv::merge(ch2, flowOut);
}

// 清理光流 NaN / Inf / 极端值
void CleanFlowNaN(cv::Mat& flow) {
    if (flow.empty() || flow.type() != CV_32FC2) return;

    std::vector<cv::Mat> parts(2);
    cv::split(flow, parts);

    for (auto& p : parts) {
        cv::patchNaNs(p, 0.0);
        cv::Mat bad;
        cv::compare(p, cv::Scalar(1e6), bad, cv::CMP_GT);
        p.setTo(0.0, bad);
        cv::compare(p, cv::Scalar(-1e6), bad, cv::CMP_LT);
        p.setTo(0.0, bad);
    }
    cv::merge(parts, flow);
}

} // namespace

// ============================================================
// 光流计算：0=Farneback, 1=DIS, 2=TV-L1
// ============================================================
void FrameInterpolator::ComputeFlow(const cv::Mat& from,
                                    const cv::Mat& to,
                                    cv::Mat& flow) const {
    if (m_params.flowBackend == 0) {
        cv::calcOpticalFlowFarneback(from, to, flow, 0.5, 3, 15, 3, 5, 1.1, 0);
        return;
    }

    // 大位移处理：粗分辨率 Farneback 作为初值（仅 DIS 用）
    cv::Mat initialFlow;
    if (m_params.maxCoarseLevels > 0 && m_params.flowBackend == 1) {
        const double coarseScale = std::pow(0.5, m_params.maxCoarseLevels);
        cv::Mat fromCoarse, toCoarse;
        cv::resize(from, fromCoarse, cv::Size(), coarseScale, coarseScale, cv::INTER_AREA);
        cv::resize(to,   toCoarse,   cv::Size(), coarseScale, coarseScale, cv::INTER_AREA);

        cv::Mat flowCoarse;
        cv::calcOpticalFlowFarneback(fromCoarse, toCoarse, flowCoarse, 0.5, 3, 15, 3, 5, 1.1, 0);
        cv::resize(flowCoarse, initialFlow, from.size(), 0, 0, cv::INTER_LINEAR);
        initialFlow *= (1.0 / coarseScale);
    }

    // ---------- 后端 2：TV-L1 ----------
    if (m_params.flowBackend == 2) {
        try {
            cv::Ptr<cv::optflow::DualTVL1OpticalFlow> tvl1 =
                cv::optflow::DualTVL1OpticalFlow::create();
            tvl1->setScalesNumber(5);
            tvl1->setWarpingsNumber(5);
            tvl1->setLambda(0.15);
            tvl1->setTau(0.25);
            tvl1->setTheta(0.3);
            tvl1->setEpsilon(0.01);
            tvl1->setUseInitialFlow(false);

            flow.release();
            tvl1->calc(from, to, flow);

            if (flow.empty() || flow.type() != CV_32FC2 || flow.size() != from.size()) {
                std::fprintf(stderr, "[TV-L1] invalid output, using zero flow\n");
                flow = cv::Mat::zeros(from.size(), CV_32FC2);
                return;
            }
            CleanFlowNaN(flow);
        } catch (const cv::Exception& e) {
            std::fprintf(stderr, "[TV-L1] exception: %s\n", e.what());
            flow = cv::Mat::zeros(from.size(), CV_32FC2);
        }
        return;
    }

    // ---------- 后端 1：DIS ----------
    int preset = cv::DISOpticalFlow::PRESET_ULTRAFAST;
    switch (m_params.disPreset) {
        case 1: preset = cv::DISOpticalFlow::PRESET_FAST;   break;
        case 2: preset = cv::DISOpticalFlow::PRESET_MEDIUM; break;
        default: preset = cv::DISOpticalFlow::PRESET_ULTRAFAST; break;
    }
    cv::Ptr<cv::DISOpticalFlow> dis = cv::DISOpticalFlow::create(preset);
    if (!initialFlow.empty()) flow = initialFlow;
    else                       flow.release();
    dis->calc(from, to, flow);
}

// ============================================================
// 时域平滑 + 光流后处理
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

    cv::Mat prevSmooth, currSmooth;
    constexpr double bs = 8.0;
    cv::bilateralFilter(prevGray, prevSmooth, 5, bs, bs);
    cv::bilateralFilter(currGray, currSmooth, 5, bs, bs);

    cv::Mat flowFwdRaw, flowBwdRaw;
    ComputeFlow(prevSmooth, currSmooth, flowFwdRaw);
    ComputeFlow(currSmooth, prevSmooth, flowBwdRaw);

    PostProcessFlow(flowFwdRaw, prevGray, flowFwd);
    PostProcessFlow(flowBwdRaw, currGray, flowBwd);

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
// 融合
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

    cv::Mat warpedDiff;
    cv::absdiff(wpF, wcF, warpedDiff);
    cv::Mat warpedDiffGray;
    cv::cvtColor(warpedDiff, warpedDiffGray, cv::COLOR_BGR2GRAY);
    cv::Mat diffMask8;
    cv::compare(warpedDiffGray, cv::Scalar(25.0), diffMask8, cv::CMP_GT);
    cv::Mat diffMask;
    diffMask8.convertTo(diffMask, CV_32F, 1.0 / 255.0);
    cv::GaussianBlur(diffMask, diffMask, cv::Size(0, 0), 1.5);

    cv::Mat combinedMask;
    cv::multiply(occWeight, diffMask, combinedMask);

    cv::Mat combinedW3 = ExpandWeightTo3Ch(combinedMask);
    cv::Mat oneMinusCombinedW3;
    cv::subtract(cv::Scalar::all(1.0), combinedW3, oneMinusCombinedW3);

    cv::Mat candidateOccluded;
    cv::add(combinedW3.mul(candidateLinear),
            oneMinusCombinedW3.mul(warpedAvg),
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

    cv::Mat coarseResult;
    result32F.convertTo(coarseResult, prev.type());

    // ★ 拉普拉斯融合：加 try/catch，失败时回退到 coarseResult
    try {
        cv::Mat result = LaplacianBlend(prev, curr, wpF, wcF, confidence, t, 4);
        return result;
    } catch (const cv::Exception& e) {
        std::fprintf(stderr, "[LaplacianBlend] exception: %s, fallback to coarse\n", e.what());
        return coarseResult;
    }
}

cv::Mat FrameInterpolator::Interpolate(const cv::Mat& prev,
                                       const cv::Mat& curr,
                                       double t) const {
    cv::Mat fF, fB;
    ComputeFlows(prev, curr, fF, fB);
    return BlendFromFlows(prev, curr, fF, fB, t);
}
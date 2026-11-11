#include "FrameInterpolator.h"

// guidedFilter 需要 ximgproc 模块（BUILD_LIST 里已有，仅头文件 include）
#include <opencv2/ximgproc.hpp>

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

cv::Mat LaplacianBlend(const cv::Mat& prev8u, const cv::Mat& curr8u,
                       const cv::Mat& wpF, const cv::Mat& wcF,
                       const cv::Mat& confidence,
                       double t, int levels = 4) {
    cv::Mat prevF, currF;
    prev8u.convertTo(prevF, CV_32FC3);
    curr8u.convertTo(currF, CV_32FC3);

    std::vector<cv::Mat> gp, gc, gwp, gwc;
    cv::buildPyramid(prevF, gp, levels);
    cv::buildPyramid(currF, gc, levels);
    cv::buildPyramid(wpF, gwp, levels);
    cv::buildPyramid(wcF, gwc, levels);

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
        lp.push_back(lapP);  lc.push_back(lapC);
        lwp.push_back(lapWP); lwc.push_back(lapWC);
    }
    lp.push_back(gp.back());  lc.push_back(gc.back());
    lwp.push_back(gwp.back()); lwc.push_back(gwc.back());

    cv::Mat conf3 = ExpandWeightTo3Ch(confidence);
    cv::Mat oneMinusConf3;
    cv::subtract(cv::Scalar::all(1.0), conf3, oneMinusConf3);

    std::vector<cv::Mat> lout;
    for (int i = 0; i < levels; ++i) {
        cv::Mat warpBlend, linearBlend;
        cv::addWeighted(lwp[i], 1.0 - t, lwc[i], t, 0.0, warpBlend);
        cv::addWeighted(lp[i],  1.0 - t, lc[i],  t, 0.0, linearBlend);

        cv::Mat blended;
        if (i == levels - 1) {
            // 最低频（大色块）：全部 linear，无 ghost
            blended = linearBlend;
        } else if (i == 0) {
            // 最高频（细节）：全部 warp，保留清晰度
            blended = warpBlend;
        } else {
            // 中间频：按 confidence 混合
            cv::add(conf3.mul(warpBlend),
                    oneMinusConf3.mul(linearBlend),
                    blended);
        }
        lout.push_back(blended);
    }

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

// 原理：
// 邻域平均：单像素误差大但邻域一致 → 仍然判定为"一致"（文本边缘不再被大面积误判）
// 动态阈值：小位移区域（文本、静止）本来就准，放宽阈值让它走 warp（保清晰）；大位移区域保持严格（避 ghost）
// 预期效果：文本区域 confidence 从 0 变成 1，走 warp 保清晰；同时大位移区仍然严格，ghost 不增加，清晰度大幅提升。
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

    // ★ 邻域平均：5x5 邻域内的平均 err²
    cv::Mat err2Local;
    cv::blur(err2, err2Local, cv::Size(5, 5));

    // ★ 动态阈值：小位移区域放宽，大位移区域收紧
    cv::Mat mag = ComputeFlowMagnitude(flowFwd);
    cv::Mat smallMag8;
    cv::compare(mag, cv::Scalar(2.0), smallMag8, cv::CMP_LT);
    cv::Mat smallMag;
    smallMag8.convertTo(smallMag, CV_32F, 1.0 / 255.0);
    cv::Mat largeMag;
    cv::subtract(cv::Scalar::all(1.0), smallMag, largeMag);

    // 小位移：阈值 = (2 * threshold)²，宽松 4 倍
    // 大位移：阈值 = threshold²，保持严格
    cv::Mat thr2Small = cv::Mat::ones(err2.size(), CV_32F) * (2.0 * threshold) * (2.0 * threshold);
    cv::Mat thr2Large = cv::Mat::ones(err2.size(), CV_32F) * threshold * threshold;

    cv::Mat maskSmall, maskLarge, maskSmall8, maskLarge8;
    cv::compare(err2Local, thr2Small, maskSmall8, cv::CMP_LT);
    cv::compare(err2Local, thr2Large, maskLarge8, cv::CMP_LT);
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

cv::Mat ExpandWeightTo3Ch(const cv::Mat& weight) {
    cv::Mat ch[3] = { weight, weight, weight };
    cv::Mat w3;
    cv::merge(ch, 3, w3);
    return w3;
}


// ============================================================
// 方案 1：光流后处理（中值 + 引导滤波）
//   目的：消除光流的孤立噪点，让光流跟随图像边缘
//   - medianBlur：去掉局部跳变（伪匹配）
//   - guidedFilter：以灰度图为引导，光流在物体边界处断开
// ============================================================
void PostProcessFlow(const cv::Mat& flowIn,
                     const cv::Mat& guideGray,
                     cv::Mat& flowOut) {
    if (flowIn.empty()) {
        flowIn.copyTo(flowOut);
        return;
    }

    // 1) 中值滤波去孤立噪点（32F 支持 ksize=3/5）
    std::vector<cv::Mat> ch(2);
    cv::split(flowIn, ch);
    cv::medianBlur(ch[0], ch[0], 5);
    cv::medianBlur(ch[1], ch[1], 5);
    cv::Mat medianFlow;
    cv::merge(ch, medianFlow);

    // 2) 引导滤波：以灰度图为 guide，让光流在边缘处保持锐利
    //    radius=8，eps=100（flow 是像素单位，量级较大，eps 也要相应大）
    std::vector<cv::Mat> ch2(2);
    cv::split(medianFlow, ch2);

    cv::Mat gf0, gf1;
    cv::ximgproc::guidedFilter(guideGray, ch2[0], gf0, 8, 100.0);
    cv::ximgproc::guidedFilter(guideGray, ch2[1], gf1, 8, 100.0);

    ch2[0] = gf0;
    ch2[1] = gf1;
    cv::merge(ch2, flowOut);
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
// 优化 5：时域平滑 + 方案 1：光流后处理
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

    // 双边滤波预处理：平滑纹理噪声
    cv::Mat prevSmooth, currSmooth;
    constexpr double bilateralSigmaColor = 8.0;
    cv::bilateralFilter(prevGray,  prevSmooth,  5, bilateralSigmaColor, bilateralSigmaColor);
    cv::bilateralFilter(currGray,  currSmooth,  5, bilateralSigmaColor, bilateralSigmaColor);

    // ★ 修复：先算到 Raw 变量，再做后处理输出到 flowFwd / flowBwd
    cv::Mat flowFwdRaw, flowBwdRaw;
    ComputeFlow(prevSmooth, currSmooth, flowFwdRaw);
    ComputeFlow(currSmooth, prevSmooth, flowBwdRaw);

    // ★ 方案 1：光流后处理（中值 + 引导滤波）
    PostProcessFlow(flowFwdRaw, prevGray, flowFwd);
    PostProcessFlow(flowBwdRaw, currGray, flowBwd);

    // ---- 时域平滑 ----
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

    // ---------- 0. 坐标网格 + 反向映射 ----------
    cv::Mat gridX, gridY;
    BuildGrid(h, w, gridX, gridY);

    cv::Mat warpedPrev = WarpToMid(prev, flowFwd, gridX, gridY,  t);
    cv::Mat warpedCurr = WarpToMid(curr, flowBwd, gridX, gridY, -(1.0 - t));

    // ---------- 1. confidence ----------
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

    cv::Mat candidateWarped;
    cv::addWeighted(wpF, 1.0 - t, wcF, t, 0.0, candidateWarped);

    // ---------- 候选 B：线性混合（静态区） ----------
    cv::Mat candidateLinear;
    cv::addWeighted(prevF, 1.0 - t, currF, t, 0.0, candidateLinear);

    // ---------- 候选 C：遮挡区 ----------
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

    cv::Mat combinedW3 = ExpandWeightTo3Ch(combinedMask);

    cv::Mat oneMinusCombinedW3;
    cv::subtract(cv::Scalar::all(1.0), combinedW3, oneMinusCombinedW3);

    cv::Mat candidateOccluded;
    cv::add(combinedW3.mul(candidateLinear),
            oneMinusCombinedW3.mul(warpedAvg),
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
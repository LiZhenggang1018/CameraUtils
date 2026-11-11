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
    // 删除 dilate，避免遮挡区边缘被误判为"一致"
    // 如果 confidence 本身噪声大，改为 erode
    // cv::erode(confidence, confidence,
    //           cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3)));
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

// 计算光流幅值（单通道）
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
//   1. 降采样到 1/(2^maxCoarseLevels)，用 Farneback 算粗光流
//   2. 上采样回全分辨率，乘以 1/coarseScale，得到粗光流初值
//   3. 传给 DIS 作为初值，DIS 内部从初值开始精修
// ============================================================
void FrameInterpolator::ComputeFlow(const cv::Mat& from,
                                    const cv::Mat& to,
                                    cv::Mat& flow) const {
    // Farneback 后端：不做大位移处理（本身自带金字塔）
    if (m_params.flowBackend == 0) {
        cv::calcOpticalFlowFarneback(from, to, flow,
            0.5, 3, 15, 3, 5, 1.1, 0);
        return;
    }

    // ---- DIS 后端 ----
    cv::Mat initialFlow;
    if (m_params.maxCoarseLevels > 0) {
        const double coarseScale = std::pow(0.5, m_params.maxCoarseLevels);
        cv::Mat fromCoarse, toCoarse;
        cv::resize(from, fromCoarse, cv::Size(), coarseScale, coarseScale, cv::INTER_AREA);
        cv::resize(to,   toCoarse,   cv::Size(), coarseScale, coarseScale, cv::INTER_AREA);

        // 粗层用 Farneback（多尺度金字塔内部已实现），得到大位移估计
        cv::Mat flowCoarse;
        cv::calcOpticalFlowFarneback(fromCoarse, toCoarse, flowCoarse,
            0.5, 3, 15, 3, 5, 1.1, 0);

        // 上采样回全分辨率，位移按 1/coarseScale 缩放
        cv::resize(flowCoarse, initialFlow, from.size(), 0, 0, cv::INTER_LINEAR);
        initialFlow *= (1.0 / coarseScale);
    }

    // 根据 disPreset 选择 DIS 预设
    int preset = cv::DISOpticalFlow::PRESET_ULTRAFAST;
    switch (m_params.disPreset) {
        case 1: preset = cv::DISOpticalFlow::PRESET_FAST;   break;
        case 2: preset = cv::DISOpticalFlow::PRESET_MEDIUM; break;
        default: preset = cv::DISOpticalFlow::PRESET_ULTRAFAST; break;
    }
    cv::Ptr<cv::DISOpticalFlow> dis = cv::DISOpticalFlow::create(preset);

    // DIS 的 calc() 支持把非空 flow 作为初值
    if (!initialFlow.empty()) {
        flow = initialFlow;
    } else {
        flow.release();
    }
    dis->calc(from, to, flow);
}

// ============================================================
// 优化 5：时域平滑
//   小位移区域（|F| < temporalMagThreshold）做 EMA，
//   大位移区域完全用新值，避免滞后。
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
    // 【新】双边滤波预处理：平滑纹理噪声，让 DIS 光流更稳定
    cv::Mat prevSmooth, currSmooth;
    cv::bilateralFilter(prevGray, prevSmooth, 5, 25.0, 25.0);
    cv::bilateralFilter(currGray, currSmooth, 5, 25.0, 25.0);

    ComputeFlow(prevSmooth, currSmooth, flowFwd);
    ComputeFlow(currSmooth, prevSmooth, flowBwd);

    // ---- 时域平滑 ----
    if (m_params.enableTemporalSmooth
        && !m_prevFlowFwd.empty()
        && m_prevFlowFwd.size() == flowFwd.size()
        && m_prevFlowFwd.type() == flowFwd.type()) {

        // 当前光流的幅值
        cv::Mat mag = ComputeFlowMagnitude(flowFwd);

        // 小位移掩膜：mag < threshold → 1
        cv::Mat smallMask8;
        cv::compare(mag, cv::Scalar(m_params.temporalMagThreshold),
                    smallMask8, cv::CMP_LT);
        cv::Mat smallMask;
        smallMask8.convertTo(smallMask, CV_32F, 1.0 / 255.0);

        // EMA 平滑光流：alpha * new + (1-alpha) * prev
        cv::Mat smoothFlow;
        cv::addWeighted(flowFwd, m_params.temporalAlpha,
                        m_prevFlowFwd, 1.0 - m_params.temporalAlpha,
                        0.0, smoothFlow);

        // 掩膜扩展到 2 通道（flow 是 CV_32FC2）
        cv::Mat mask3;
        cv::Mat ch[2] = { smallMask, smallMask };
        cv::merge(ch, 2, mask3);
        cv::Mat invMask3;
        cv::subtract(cv::Scalar::all(1.0), mask3, invMask3);

        // 小位移区用 smooth，大位移区用 new
        cv::Mat flowFwdSmoothed;
        cv::add(mask3.mul(smoothFlow), invMask3.mul(flowFwd), flowFwdSmoothed);
        flowFwd = flowFwdSmoothed;
    }

    // 更新缓存（给下一对帧用）
    m_prevFlowFwd = flowFwd.clone();
}

// ============================================================
// 融合：向量化版本（优化 1 + 2 + 3 保留）
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

    // 效果：每个像素选一侧（偏向 70%~100% 那侧），而不是 50/50 混合。ghost 从"平均双影"变成"局部单影"——主观上清晰度大幅提升。
    // ============================================================
    // 【Ghost 修复 1】非遮挡区不再 50/50 平均
    // 改用"局部流一致性"加权：
    //   - 流一致性高的点 → 接近 50/50（两个采样准）
    //   - 流一致性中等的点 → 向更可信的一侧偏（比如 70/30）
    //   - 流一致性低的点 → 完全不用这一侧（走候选 B/C）
    // ============================================================
    cv::Mat candidateWarped;

    if (t == 0.5) {
        // t=0.5 时特殊处理：不平均，取"局部误差更小的一侧"
        // 用 3x3 邻域内 |wp - wc| 的均值做"局部误差"
        cv::Mat localDiff;
        cv::Mat diffForLocal;
        cv::absdiff(wpF, wcF, diffForLocal);
        cv::cvtColor(diffForLocal, localDiff, cv::COLOR_BGR2GRAY);
        cv::blur(localDiff, localDiff, cv::Size(5, 5));

        // 每个像素，比较 wpF 和 wcF 与邻域的"局部平滑度"
        // 靠近局部均值的更可信；偏离的更可能是错采
        cv::Mat wpBlur, wcBlur;
        cv::blur(wpF, wpBlur, cv::Size(5, 5));
        cv::blur(wcF, wcBlur, cv::Size(5, 5));

        cv::Mat wpDev, wcDev;
        cv::absdiff(wpF, wpBlur, wpDev);
        cv::absdiff(wcF, wcBlur, wcDev);
        cv::Mat wpDevGray, wcDevGray;
        cv::cvtColor(wpDev, wpDevGray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(wcDev, wcDevGray, cv::COLOR_BGR2GRAY);

        // wpDev < wcDev → wp 更可信 → 权重偏 wp
        cv::Mat weightBias;
        cv::compare(wpDevGray, wcDevGray, weightBias, cv::CMP_LT);   // 1 = 偏 wp
        cv::Mat weightBias3;
        weightBias.convertTo(weightBias, CV_32F, 1.0 / 255.0);
        // 平滑过渡，避免硬边界
        cv::GaussianBlur(weightBias, weightBias, cv::Size(0, 0), 3.0);
        cv::Mat weightBiasArr[3] = { weightBias, weightBias, weightBias };
        cv::merge(weightBiasArr, 3, weightBias3);

        // 偏向权重：0.75/0.25 而非 0.5/0.5（降低平均带来的 ghost）
        cv::Mat oneMinusBias;
        cv::subtract(cv::Scalar::all(1.0), weightBias3, oneMinusBias);
        cv::add(weightBias3.mul(wpF), oneMinusBias.mul(wcF), candidateWarped);
    } else {
        // 非 t=0.5 时，保持按 t 加权（时间上更接近哪一帧就信哪一帧）
        cv::addWeighted(wpF, 1.0 - t, wcF, t, 0.0, candidateWarped);
    }

    cv::Mat candidateLinear;
    cv::addWeighted(prevF, 1.0 - t, currF, t, 0.0, candidateLinear);

    cv::Mat magFwd = ComputeFlowMagnitude(flowFwd);
    cv::Mat occWeight8;
    cv::compare(magFwd, cv::Scalar(m_params.staticMagThreshold),
                occWeight8, cv::CMP_GT);
    cv::Mat occWeight;
    occWeight8.convertTo(occWeight, CV_32F, 1.0 / 255.0);
    if (m_params.occlusionBlurSigma > 0.0) {
        cv::GaussianBlur(occWeight, occWeight,
                         cv::Size(0, 0), m_params.occlusionBlurSigma);
    }

    // 【Ghost 修复】遮挡区不再用 0.5*wp + 0.5*wc，改成：
    //   |wp - wc| 大的区域（真遮挡）→ candidateLinear（模糊但无 ghost）
    //   |wp - wc| 小的区域（伪遮挡）→ warpedAvg（清晰）
    cv::Mat warpedAvg;
    cv::addWeighted(wpF, 0.5, wcF, 0.5, 0.0, warpedAvg);

    // 双向 warp 的差异（3 通道）
    cv::Mat warpedDiff;
    cv::absdiff(wpF, wcF, warpedDiff);

    // 转灰度：差异超过 25（0~255）就算真遮挡
    cv::Mat warpedDiffGray;
    cv::cvtColor(warpedDiff, warpedDiffGray, cv::COLOR_BGR2GRAY);

    cv::Mat diffMask8;
    cv::compare(warpedDiffGray, cv::Scalar(25.0), diffMask8, cv::CMP_GT);
    cv::Mat diffMask;
    diffMask8.convertTo(diffMask, CV_32F, 1.0 / 255.0);

    // 轻微模糊，避免硬边界
    cv::GaussianBlur(diffMask, diffMask, cv::Size(0, 0), 1.5);

    cv::Mat diffW3 = ExpandWeightTo3Ch(diffMask);
    cv::Mat oneMinusDiffW3;
    cv::subtract(cv::Scalar::all(1.0), diffW3, oneMinusDiffW3);

    // 效果：combinedMask = 0.5 的过渡区，权重从 0.5 变成 0.25（linear 权重），混合的双影大幅减少。
    // ============================================================
    // 【Ghost 修复 2】遮挡区直接用 candidateLinear
    // warpedAvg 在遮挡区仍有 50/50 平均的双影
    // 干脆：只有"很轻微遮挡"才用 warpedAvg，"明显遮挡"完全 linear
    // ============================================================
    cv::Mat candidateOccluded;

    // combinedMask 的平方：把"中等遮挡"推向 linear
    // combined^2 让 0.5 → 0.25，0.8 → 0.64，1.0 → 1.0
    cv::Mat combinedSq;
    cv::multiply(combinedW3, combinedW3, combinedSq);

    cv::Mat oneMinusCombinedSq;
    cv::subtract(cv::Scalar::all(1.0), combinedSq, oneMinusCombinedSq);

    cv::add(combinedSq.mul(candidateLinear),
            oneMinusCombinedSq.mul(warpedAvg),
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
        // ============================================================
    // 【Ghost 修复 3】后处理：对遮挡区做一次轻量 bilateral 平滑
    // 遮挡区的 ghost 本质是高频双影，bilateral 在保留边缘的同时
    // 抑制这些小范围的双影结构
    // ============================================================
    if (m_params.enableStaticProtect) {
        // 只在遮挡区做 bilateral，非遮挡区保持原样
        cv::Mat confMask8;
        cv::compare(confidence, cv::Scalar(0.3), confMask8, cv::CMP_LT);
        cv::Mat confMask;
        confMask8.convertTo(confMask, CV_32F, 1.0 / 255.0);
        cv::Mat confMask3 = ExpandWeightTo3Ch(confMask);

        cv::Mat smoothed;
        cv::Mat result8u;
        result32F.convertTo(result8u, CV_8UC3);
        cv::bilateralFilter(result8u, smoothed, 5, 30.0, 30.0);
        cv::Mat smoothedF;
        smoothed.convertTo(smoothedF, CV_32FC3);

        cv::Mat oneMinusConfMask3;
        cv::subtract(cv::Scalar::all(1.0), confMask3, oneMinusConfMask3);
        cv::add(confMask3.mul(smoothedF),
                oneMinusConfMask3.mul(result32F),
                result32F);
    }
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
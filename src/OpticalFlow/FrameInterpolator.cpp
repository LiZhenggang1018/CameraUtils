#include "FrameInterpolator.h"

namespace {

// 构建坐标网格：gridX[y][x] = x，gridY[y][x] = y
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

// 【优化 1】向量化反向映射：
//   out(p) = from(p - scale * flow(p))
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

// 【优化 1】向量化一致性掩膜：
//   err² = ||F_fwd(p) + F_bwd(p + F_fwd(p))||²
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

// 【优化 2】向量化静态掩膜：|F_fwd| < threshold → 1
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

// 单通道权重扩展成三通道
cv::Mat ExpandWeightTo3Ch(const cv::Mat& weight) {
    cv::Mat ch[3] = { weight, weight, weight };
    cv::Mat w3;
    cv::merge(ch, 3, w3);
    return w3;
}

} // namespace

// 【优化 3】DIS 换 PRESET_ULTRAFAST，且每次新建实例
// 老版本 OpenCV 的 static DIS 在连续 calc 时可能保留内部状态 → 返回全零光流
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

cv::Mat FrameInterpolator::Interpolate(const cv::Mat& prev,
                                       const cv::Mat& curr,
                                       double t) const {
    CV_Assert(!prev.empty() && !curr.empty());
    CV_Assert(prev.size() == curr.size());
    CV_Assert(prev.type() == curr.type());
    CV_Assert(t > 0.0 && t < 1.0);

    const int h = prev.rows;
    const int w = prev.cols;

    // ---------- 1. 灰度化 ----------
    cv::Mat prevGray, currGray;
    if (prev.channels() == 3) {
        cv::cvtColor(prev, prevGray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(curr, currGray, cv::COLOR_BGR2GRAY);
    } else {
        prevGray = prev;
        currGray = curr;
    }

    // ---------- 2. 双向光流 ----------
    cv::Mat flowFwd, flowBwd;
    ComputeFlow(prevGray, currGray, flowFwd);
    ComputeFlow(currGray, prevGray, flowBwd);

    // ---------- 3. 坐标网格（只构建一次） ----------
    cv::Mat gridX, gridY;
    BuildGrid(h, w, gridX, gridY);

    // ---------- 4. 反向映射到 t 时刻 ----------
    cv::Mat warpedPrev = WarpToMid(prev, flowFwd, gridX, gridY,  t);
    cv::Mat warpedCurr = WarpToMid(curr, flowBwd, gridX, gridY, -(1.0 - t));

    // ---------- 5. 一致区 confidence ----------
    cv::Mat confidence = ComputeConfidence(
        flowFwd, flowBwd, gridX, gridY, m_params.fbThreshold);
    if (m_params.confidenceBlurSigma > 0.0) {
        // 【优化 4】软化 confidence：硬 0/1 → 高斯模糊后 0~1，遮挡边界平滑过渡
        cv::GaussianBlur(confidence, confidence,
                         cv::Size(0, 0), m_params.confidenceBlurSigma);
    }

    // ---------- 6. 静态区掩膜 ----------
    cv::Mat staticMask;
    if (m_params.enableStaticProtect) {
        // 【优化 2】|F| < threshold 的区域识别为静态
        staticMask = ComputeStaticMask(flowFwd, m_params.staticMagThreshold);
        cv::GaussianBlur(staticMask, staticMask, cv::Size(0, 0), 2.0);
    } else {
        staticMask = cv::Mat::zeros(h, w, CV_32F);
    }

    // ---------- 7. 三路候选 ----------
    cv::Mat wpF, wcF, prevF, currF;
    warpedPrev.convertTo(wpF, CV_32FC3);
    warpedCurr.convertTo(wcF, CV_32FC3);
    prev.convertTo(prevF, CV_32FC3);
    curr.convertTo(currF, CV_32FC3);

    // 候选 A：光流加权（一致区）
    cv::Mat candidateWarped;
    cv::addWeighted(wpF, 1.0 - t, wcF, t, 0.0, candidateWarped);

    // 候选 B：线性混合（静态区）
    cv::Mat candidateLinear;
    cv::addWeighted(prevF, 1.0 - t, currF, t, 0.0, candidateLinear);

    // 候选 C：遮挡区
    // 【优化 4】遮挡区双向采样：
    //   |F_fwd| 大（真运动遮挡）→ warpedAvg = 0.5*wp + 0.5*wc
    //   |F_fwd| 小（光流噪声导致的伪遮挡）→ 退化为线性混合
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

    cv::Mat occW3   = ExpandWeightTo3Ch(occWeight);
    cv::Mat oneMinusOccW3;
    cv::subtract(cv::Scalar::all(1.0), occW3, oneMinusOccW3);

    cv::Mat candidateOccluded;
    cv::add(occW3.mul(warpedAvg),
            oneMinusOccW3.mul(candidateLinear),
            candidateOccluded);

    // ---------- 8. 三区加权融合（全部向量化） ----------
    cv::Mat conf3   = ExpandWeightTo3Ch(confidence);
    cv::Mat static3 = ExpandWeightTo3Ch(staticMask);

    cv::Mat oneMinusConf3;
    cv::subtract(cv::Scalar::all(1.0), conf3, oneMinusConf3);
    cv::Mat oneMinusStatic3;
    cv::subtract(cv::Scalar::all(1.0), static3, oneMinusStatic3);

    // 非静态 = conf * warped + (1-conf) * occluded
    cv::Mat nonStatic;
    cv::add(conf3.mul(candidateWarped),
            oneMinusConf3.mul(candidateOccluded),
            nonStatic);

    // 最终 = static * linear + (1-static) * nonStatic
    cv::Mat result32F;
    cv::add(static3.mul(candidateLinear),
            oneMinusStatic3.mul(nonStatic),
            result32F);

    // ---------- 9. 转回原类型 ----------
    cv::Mat result;
    result32F.convertTo(result, prev.type());
    return result;
}
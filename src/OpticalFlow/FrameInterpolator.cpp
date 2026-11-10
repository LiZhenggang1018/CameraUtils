#include "FrameInterpolator.h"

namespace {

// 构造/复用全图坐标网格：gridX[y][x] = x, gridY[y][x] = y
// 每次调用都重新创建，避免线程安全问题；开销相对后续步骤可忽略
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

// 用 flow 把 from 反向映射到中间时刻 t 的坐标系：
//   mapX = x - scale * flow.x
//   mapY = y - scale * flow.y
cv::Mat WarpToMid(const cv::Mat& from,
                  const cv::Mat& flow,
                  const cv::Mat& gridX,
                  const cv::Mat& gridY,
                  double scale) {
    std::vector<cv::Mat> flowParts(2);
    cv::split(flow, flowParts);

    cv::Mat mapX = gridX - scale * flowParts[0];   // MatExpr：全图 SIMD 减法
    cv::Mat mapY = gridY - scale * flowParts[1];

    cv::Mat out;
    cv::remap(from, out, mapX, mapY,
              cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    return out;
}

// 一致性置信度：全部向量化
cv::Mat ComputeConfidence(const cv::Mat& flowFwd,
                          const cv::Mat& flowBwd,
                          const cv::Mat& gridX,
                          const cv::Mat& gridY,
                          double threshold) {
    // 1. 在 p + F_{0→1}(p) 处采样 F_{1→0}
    std::vector<cv::Mat> fwdParts(2);
    cv::split(flowFwd, fwdParts);

    cv::Mat mapX = gridX + fwdParts[0];
    cv::Mat mapY = gridY + fwdParts[1];

    cv::Mat bwdAtFwd;
    cv::remap(flowBwd, bwdAtFwd, mapX, mapY,
              cv::INTER_LINEAR, cv::BORDER_REPLICATE);

    // 2. err² = (fx + bx)² + (fy + by)²
    cv::Mat sumFlow;
    cv::add(flowFwd, bwdAtFwd, sumFlow);

    std::vector<cv::Mat> sumParts(2);
    cv::split(sumFlow, sumParts);

    cv::Mat err2 = sumParts[0].mul(sumParts[0]) + sumParts[1].mul(sumParts[1]);

    // 3. mask = (err² < threshold²) ? 1 : 0
    const double thr2 = threshold * threshold;
    cv::Mat mask8;
    cv::compare(err2, cv::Scalar(thr2), mask8, cv::CMP_LT);

    cv::Mat confidence;
    mask8.convertTo(confidence, CV_32F, 1.0 / 255.0);

    // 4. 膨胀一次，减少硬边界
    cv::dilate(confidence, confidence,
               cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3)));
    return confidence;
}

} // namespace

void FrameInterpolator::ComputeFlow(const cv::Mat& from,
                                    const cv::Mat& to,
                                    cv::Mat& flow) const {
    if (m_params.flowBackend == 1) {
        static cv::Ptr<cv::DISOpticalFlow> dis =
            cv::DISOpticalFlow::create(cv::DISOpticalFlow::PRESET_MEDIUM);
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

    // 1. 灰度化
    cv::Mat prevGray, currGray;
    if (prev.channels() == 3) {
        cv::cvtColor(prev, prevGray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(curr, currGray, cv::COLOR_BGR2GRAY);
    } else {
        prevGray = prev;
        currGray = curr;
    }

    // 2. 双向光流
    cv::Mat flowFwd, flowBwd;
    ComputeFlow(prevGray, currGray, flowFwd);
    ComputeFlow(currGray, prevGray, flowBwd);

    // 3. 全图坐标网格（一次构建，后续复用）
    const int h = prev.rows;
    const int w = prev.cols;
    cv::Mat gridX, gridY;
    BuildGrid(h, w, gridX, gridY);

    // 4. 反向映射到中间时刻
    cv::Mat warpedPrev = WarpToMid(prev, flowFwd, gridX, gridY,  t);
    cv::Mat warpedCurr = WarpToMid(curr, flowBwd, gridX, gridY, -(1.0 - t));

    // 5. 一致性置信度
    cv::Mat confidence = ComputeConfidence(
        flowFwd, flowBwd, gridX, gridY, m_params.fbThreshold);

    // 6. 融合（全部向量化）
    cv::Mat wpF, wcF;
    warpedPrev.convertTo(wpF, CV_32FC3);
    warpedCurr.convertTo(wcF, CV_32FC3);

    cv::Mat blended;
    cv::addWeighted(wpF, 1.0 - t, wcF, t, 0.0, blended);   // (1-t)*wp + t*wc

    cv::Mat conf3;
    cv::Mat chs[3] = { confidence, confidence, confidence };
    cv::merge(chs, 3, conf3);                               // 单通道 -> 三通道

    cv::Mat oneMinusConf3;
    cv::subtract(cv::Scalar::all(1.0), conf3, oneMinusConf3);

    cv::Mat outF;
    cv::add(conf3.mul(blended), oneMinusConf3.mul(wcF), outF);

    cv::Mat result;
    outF.convertTo(result, prev.type());
    return result;
}
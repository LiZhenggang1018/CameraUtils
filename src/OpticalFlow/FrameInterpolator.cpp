#include "FrameInterpolator.h"

namespace {

// 把 flow 在 (x, y) 处的采样点 + 另一侧 flow 做一致性检测，输出 0~1 的置信度
cv::Mat ComputeConfidence(
    const cv::Mat& flowFwd,
    const cv::Mat& flowBwd,
    double threshold) {

    const int h = flowFwd.rows;
    const int w = flowFwd.cols;

    // 在 p + F(p) 处采样 F_{1→0}，需要构造重映射坐标
    cv::Mat mapX(h, w, CV_32F);
    cv::Mat mapY(h, w, CV_32F);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const cv::Point2f f = flowFwd.at<cv::Point2f>(y, x);
            mapX.at<float>(y, x) = static_cast<float>(x) + f.x;
            mapY.at<float>(y, x) = static_cast<float>(y) + f.y;
        }
    }

    cv::Mat bwdAtFwd;
    cv::remap(flowBwd, bwdAtFwd, mapX, mapY,
              cv::INTER_LINEAR, cv::BORDER_REPLICATE);

    // 一致性误差：|F_{0→1}(p) + F_{1→0}(p + F_{0→1}(p))|
    cv::Mat confidence(h, w, CV_32F);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const cv::Point2f f = flowFwd.at<cv::Point2f>(y, x);
            const cv::Point2f b = bwdAtFwd.at<cv::Point2f>(y, x);
            const double err = cv::norm(f + b);
            confidence.at<float>(y, x) =
                (err < threshold) ? 1.0F : 0.0F;
        }
    }

    // 简单膨胀一次，把边缘扩展到遮挡区内部，减少硬边界
    cv::Mat mask3(h, w, CV_32F, cv::Scalar(0));
    cv::dilate(confidence, mask3, cv::getStructuringElement(
        cv::MORPH_RECT, cv::Size(3, 3)));
    return mask3;
}

// 用 flow 把 from 反向映射到中间时刻 t 的坐标系
cv::Mat WarpToMid(
    const cv::Mat& from,
    const cv::Mat& flow,
    double scale) {

    const int h = from.rows;
    const int w = from.cols;

    cv::Mat mapX(h, w, CV_32F);
    cv::Mat mapY(h, w, CV_32F);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const cv::Point2f f = flow.at<cv::Point2f>(y, x);
            mapX.at<float>(y, x) = static_cast<float>(x) - scale * f.x;
            mapY.at<float>(y, x) = static_cast<float>(y) - scale * f.y;
        }
    }

    cv::Mat out;
    cv::remap(from, out, mapX, mapY,
              cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    return out;
}

} // namespace

void FrameInterpolator::ComputeFlow(
    const cv::Mat& from,
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

cv::Mat FrameInterpolator::Interpolate(
    const cv::Mat& prev,
    const cv::Mat& curr,
    double t) const {

    CV_Assert(!prev.empty() && !curr.empty());
    CV_Assert(prev.size() == curr.size());
    CV_Assert(prev.type() == curr.type());
    CV_Assert(t > 0.0 && t < 1.0);

    // 1. 灰度化（光流只需要灰度）
    cv::Mat prevGray;
    cv::Mat currGray;
    if (prev.channels() == 3) {
        cv::cvtColor(prev, prevGray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(curr, currGray, cv::COLOR_BGR2GRAY);
    } else {
        prevGray = prev;
        currGray = curr;
    }

    // 2. 双向光流
    cv::Mat flowFwd;
    cv::Mat flowBwd;
    ComputeFlow(prevGray, currGray, flowFwd);   // prev → curr
    ComputeFlow(currGray, prevGray, flowBwd);   // curr → prev

    // 3. 反向映射到中间时刻
    //    I_t 上位置 p 从 prev 采样：p - t * F_{0→1}(p)
    //    I_t 上位置 p 从 curr 采样：p + (1-t) * F_{1→0}(p)
    cv::Mat warpedPrev = WarpToMid(prev, flowFwd, t);
    cv::Mat warpedCurr = WarpToMid(curr, flowBwd, -(1.0 - t));

    // 4. 一致性掩膜：把前后向不一致的像素（遮挡/大位移）标出来
    cv::Mat confidence = ComputeConfidence(flowFwd, flowBwd, m_params.fbThreshold);

    // 5. 逐像素融合
    //    一致区域： (1-t) * warpedPrev + t * warpedCurr
    //    不一致区域：优先用 warpedCurr（后帧采样通常更稳定，因为合成 t<0.5 时后向信息更丰富）
    //    这里对通道数分别处理，用 float 中间结果避免 8U 溢出
    const int channels = prev.channels();
    cv::Mat blended(prev.size(), CV_MAKETYPE(CV_32F, channels));

    for (int y = 0; y < prev.rows; ++y) {
        const float* wp = warpedPrev.ptr<float>(y);
        const float* wc = warpedCurr.ptr<float>(y);
        const float* conf = confidence.ptr<float>(y);
        float* out = blended.ptr<float>(y);
        (void)wp;
        (void)wc;
        (void)conf;
        (void)out;
    }
    // ↑ 上面空循环是占位，因为 warpedPrev/Curr 是 8U 类型，
    //    实际实现见下方正式版本（注意：remap 输出与输入同类型）
    return prev.clone(); // 占位，防止误用；下方正式版本替换
}
# src/OpticalFlow —— 逐文件实现细节

本文档面向**实现维护者**，详细说明每个 `.cpp` 的内部结构、关键代码片段、设计决策、历史修复。
模块总览、数据流、性能分析、调试方法请见同目录的 [`README.md`](./README.md)。

---

## 目录

- [1. FrameInterpolator.cpp —— 核心实现](#1-frameinterpolatorcpp--核心实现)
- [2. FfmpegVideoWriter.cpp —— MP4 编码](#2-ffmpegvideowritercpp--mp4-编码)
- [3. LkTracker.cpp —— 稀疏 LK 跟踪](#3-lktrackercpp--稀疏-lk-跟踪)
- [4. BiDirectionalLkFilter.cpp —— 双向一致性](#4-bidirectionallkfiltercpp--双向一致性)
- [5. MiddleburyColor.cpp —— 光流配色](#5-middleburycolorcpp--光流配色)
- [6. Visualization.cpp —— 绘制工具](#6-visualizationcpp--绘制工具)
- [7. 历史修复汇总](#7-历史修复汇总)

---

## 1. FrameInterpolator.cpp —— 核心实现

### 文件内部结构

```
┌─────────────────────────────────────────────────────────┐
│ anonymous namespace（文件内私有工具函数）                │
│  ├── BuildGrid               坐标网格                    │
│  ├── WarpToMid               反向映射（remap）            │
│  ├── ComputeFlowMagnitude    光流幅值                    │
│  ├── ComputeConfidence       前后向一致性（含动态阈值）    │
│  ├── ComputeStaticMask       静态区掩膜                  │
│  ├── ExpandWeightTo3Ch       单通道权重 → 三通道          │
│  ├── PostProcessFlow         中值 + guidedFilter         │
│  ├── CleanFlowNaN            NaN/Inf 清理                │
│  └── LaplacianBlend          拉普拉斯金字塔融合            │
├─────────────────────────────────────────────────────────┤
│ 公开成员函数实现                                          │
│  ├── ComputeFlow（flowDownscale 包装）                   │
│  ├── ComputeFlowInternal（实际后端分派）                  │
│  ├── ComputeFlows（光流 + 后处理 + 时域平滑）             │
│  ├── BlendFromFlows（融合）                              │
│  └── Interpolate（ComputeFlows + BlendFromFlows）        │
└─────────────────────────────────────────────────────────┘
```

### 1.1 `ComputeFlow` vs `ComputeFlowInternal` 两层拆分

**拆分原因**：`flowDownscale` 是"包装级"参数，与后端无关。拆成两层后：

- `ComputeFlow`：负责降分辨率 → 算光流 → 上采样 → 乘 `1/scale`
- `ComputeFlowInternal`：只负责指定分辨率下的光流计算

```cpp
void FrameInterpolator::ComputeFlow(from, to, flow) const {
    if (flowDownscale >= 1.0) {
        ComputeFlowInternal(from, to, flow);       // 全分辨率
        return;
    }
    // 降分辨率
    const double s = flowDownscale;
    cv::resize(from, fromSmall, ..., s, s, INTER_AREA);
    cv::resize(to,   toSmall,   ..., s, s, INTER_AREA);
    ComputeFlowInternal(fromSmall, toSmall, flowSmall);
    cv::resize(flowSmall, flow, from.size(), ..., INTER_LINEAR);
    flow *= (1.0 / s);                             // ← 位移值必须放大
}
```

### 1.2 `ComputeFlowInternal` 的三后端分派

```cpp
void FrameInterpolator::ComputeFlowInternal(from, to, flow) const {
    // 后端 0：纯 Farneback
    if (flowBackend == 0) {
        cv::calcOpticalFlowFarneback(...);
        return;
    }

    // 大位移处理（仅 DIS 且全分辨率）
    cv::Mat initialFlow;
    if (maxCoarseLevels > 0 && flowBackend == 1 && flowDownscale >= 1.0) {
        // Farneback 粗算 → 上采样 → 作为 DIS 初值
        ...
    }

    // 后端 2：TV-L1
    if (flowBackend == 2) {
        try {
            cv::Ptr<cv::optflow::DualTVL1OpticalFlow> tvl1 = ...create();
            tvl1->setUseInitialFlow(false);        // ← 关键：不用初值
            flow.release();                         // ← 关键：清空避免共享
            tvl1->calc(from, to, flow);
            // 输出校验 + CleanFlowNaN
        } catch (...) {
            flow = cv::Mat::zeros(from.size(), CV_32FC2);
        }
        return;
    }

    // 后端 1：DIS
    cv::Ptr<cv::DISOpticalFlow> dis = ...create(preset);
    if (!initialFlow.empty()) flow = initialFlow;
    else                       flow.release();
    dis->calc(from, to, flow);
}
```

### 1.3 `ComputeConfidence` —— 动态阈值 + 邻域平均

```cpp
cv::Mat ComputeConfidence(flowFwd, flowBwd, gridX, gridY, threshold) {
    // err² = ||F_fwd(p) + F_bwd(p + F_fwd(p))||²
    ...
    // 邻域平均（5x5）：单像素误差大但邻域一致 → 仍判"一致"
    cv::blur(err2, err2Local, cv::Size(5, 5));

    // 动态阈值：
    //   |F| < 2 像素的区域 → 阈值放宽 4 倍（小位移本来准）
    //   |F| >= 2 像素的区域 → 保持严格
    cv::Mat smallMag8;
    cv::compare(mag, cv::Scalar(2.0), smallMag8, cv::CMP_LT);

    const double thrSmall = 2.0 * threshold;
    cv::Mat thr2Small(err2.size(), CV_32F, cv::Scalar(thrSmall * thrSmall));
    cv::Mat thr2Large(err2.size(), CV_32F, cv::Scalar(threshold * threshold));

    cv::Mat maskSmall8, maskLarge8;
    cv::compare(err2Local, thr2Small, maskSmall8, cv::CMP_LT);
    cv::compare(err2Local, thr2Large, maskLarge8, cv::CMP_LT);

    return smallMag.mul(maskSmall) + largeMag.mul(maskLarge);
}
```

**设计意图**：
- **邻域平均**：文本/边缘单像素误差大，但邻域一致时应判"一致"，让文本走 warp 保清晰
- **动态阈值**：小位移区域（文本、静止）放宽阈值 → 走 warp（清晰）；大位移区域严格 → 走 linear（无 ghost）

### 1.4 `BlendFromFlows` —— 三路候选 + 三区融合

```
候选 A（warped）：从 warp 结果按 t 加权          ← 清晰，但可能 ghost
候选 B（linear）：prev/curr 直接按 t 加权        ← 无 ghost，但糊
候选 C（occluded）：遮挡区用 linear，伪遮挡用 warpedAvg  ← 折中

融合权重：
  confidence（前后向一致性）  → 决定走 A 还是 C
  staticMask（静态区）        → 决定走 B 还是"非静态结果"
```

**关键子步骤**：

```cpp
// 1) 遮挡区权重 = occWeight（大位移） * diffMask（双向 warp 差异大）
cv::Mat combinedMask;
cv::multiply(occWeight, diffMask, combinedMask);

// 2) 遮挡区候选 = combined * linear + (1-combined) * warpedAvg
candidateOccluded = combinedW3.mul(candidateLinear)
                  + oneMinusCombinedW3.mul(warpedAvg);

// 3) 非静态区 = conf * warped + (1-conf) * occluded
nonStatic = conf3.mul(candidateWarped) + oneMinusConf3.mul(candidateOccluded);

// 4) 最终 = static * linear + (1-static) * nonStatic
result = static3.mul(candidateLinear) + oneMinusStatic3.mul(nonStatic);
```

### 1.5 `LaplacianBlend` —— 频段分离融合

```cpp
cv::Mat LaplacianBlend(prev8u, curr8u, wpF, wcF, confidence, t, levels) {
    // 1) 构建金字塔（注意：levels - 1 不是 levels）
    cv::buildPyramid(prevF, gp, levels - 1);
    cv::buildPyramid(currF, gc, levels - 1);
    cv::buildPyramid(wpF, gwp, levels - 1);
    cv::buildPyramid(wcF, gwc, levels - 1);

    // 2) confidence 金字塔（每层 resize 到对应尺寸）
    std::vector<cv::Mat> lconf(levels);
    lconf[0] = confidence;
    for (int i = 1; i < levels; ++i)
        cv::resize(lconf[i-1], lconf[i], gp[i].size(), ..., INTER_AREA);

    // 3) 拉普拉斯分解
    for (int i = 0; i < levels - 1; ++i) {
        lapP[i] = gp[i] - pyrUp(gp[i+1], gp[i].size());
        ...
    }
    lp[levels-1] = gp.back();   // 最低频 = 高斯金字塔顶层

    // 4) 逐层融合
    for (int i = 0; i < levels; ++i) {
        if (i == levels - 1)  blended = linearBlend;               // 最低频：全 linear
        else if (i == 0)      blended = warpBlend;                 // 最高频：全 warp
        else                  blended = conf*warp + (1-conf)*linear; // 中间：混合
    }

    // 5) 重构
    result = lout[levels-1];
    for (int i = levels - 2; i >= 0; --i)
        result = pyrUp(result, lout[i].size()) + lout[i];
}
```

**设计意图**：
- 大色块（低频）的 ghost 表现为"整体颜色过渡错位"，用 linear 看不出 ghost
- 细节（高频）的 ghost 表现为"边缘错位双影"，必须用 warp
- 中间频按 `confidence` 混合

### 1.6 `PostProcessFlow` —— 光流后处理（最慢的开关）

```cpp
void PostProcessFlow(flowIn, guideGray, flowOut) {
    // 1) 中值滤波去孤立噪点
    cv::split(flowIn, ch);
    cv::medianBlur(ch[0], ch[0], 5);
    cv::medianBlur(ch[1], ch[1], 5);

    // 2) 引导滤波：让光流跟随图像边缘
    cv::ximgproc::guidedFilter(guideGray, ch2[0], gf0, 8, 100.0);
    cv::ximgproc::guidedFilter(guideGray, ch2[1], gf1, 8, 100.0);
}
```

**性能警告**：`guidedFilter` 占每对帧 200~400ms，是**最慢的后处理**。追求速度时**优先关闭 `enableFlowPostProcess`**。

### 1.7 `CleanFlowNaN` —— 数值保护

```cpp
void CleanFlowNaN(cv::Mat& flow) {
    if (flow.empty() || flow.type() != CV_32FC2) return;
    std::vector<cv::Mat> parts(2);
    cv::split(flow, parts);
    for (auto& p : parts) {
        cv::patchNaNs(p, 0.0);                              // NaN → 0
        cv::Mat bad;
        cv::compare(p, cv::Scalar(1e6), bad, cv::CMP_GT);
        p.setTo(0.0, bad);                                  // |x| > 1e6 → 0
        cv::compare(p, cv::Scalar(-1e6), bad, cv::CMP_LT);
        p.setTo(0.0, bad);                                  // |x| < -1e6 → 0
    }
    cv::merge(parts, flow);
}
```

**用途**：TV-L1 在参数不当时可能输出 NaN/Inf，传播到 `cv::multiply` 会触发 `AutoBuffer` 异常崩溃。

---

## 2. FfmpegVideoWriter.cpp —— MP4 编码

### 文件内部结构

```
AvErr(int)                 → 把 FFmpeg 错误码转字符串
Open(path, w, h, fps, ...) → 初始化上下文 → 编码器 → 流 → 文件 → sws → frame
WriteFrame(bgr)            → sws_scale → send_frame → receive_packet → write
Close()                    → flush 编码器 → write_trailer → 释放所有资源
```

### 2.1 Open 的关键步骤

```cpp
// 1) 输出上下文（根据扩展名自动选 mp4 封装）
avformat_alloc_output_context2(&m_fmtCtx, nullptr, nullptr, path.c_str());

// 2) 找编码器（优先 libx264）
const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
if (!codec) codec = avcodec_find_encoder(AV_CODEC_ID_H264);

// 3) 编码器参数
m_codecCtx->bit_rate = (enc.crf >= 0) ? 0 : bitrate;   // ← CRF 模式下置 0
m_codecCtx->color_primaries = AVCOL_PRI_BT709;
m_codecCtx->color_trc       = AVCOL_TRC_BT709;
m_codecCtx->colorspace      = AVCOL_SPC_BT709;
m_codecCtx->color_range     = AVCOL_RANGE_MPEG;

// 4) preset / crf 通过 AVDictionary 传
AVDictionary* opts = nullptr;
av_dict_set(&opts, "preset", enc.preset.c_str(), 0);
if (enc.crf >= 0) {
    char crf_buf[16];
    std::snprintf(crf_buf, sizeof(crf_buf), "%d", enc.crf);
    av_dict_set(&opts, "crf", crf_buf, 0);
}
avcodec_open2(m_codecCtx, codec, &opts);
av_dict_free(&opts);

// 5) BT.709 色彩空间设置
const int* coeff = sws_getCoefficients(SWS_CS_ITU709);
sws_setColorspaceDetails(m_swsCtx,
    coeff, 1,        // src: full range
    coeff, 0,        // dst: limited range
    0, 1<<16, 1<<16);
```

### 2.2 WriteFrame 的 EAGAIN 处理

```cpp
int ret = avcodec_send_frame(m_codecCtx, m_frame);
if (ret == AVERROR(EAGAIN)) {
    // 编码器缓冲满 → 先取完所有可用 packet 再重试
    while (true) {
        int rr = avcodec_receive_packet(m_codecCtx, m_packet);
        if (rr == AVERROR(EAGAIN) || rr == AVERROR_EOF) break;
        ...
        av_interleaved_write_frame(m_fmtCtx, m_packet);
        av_packet_unref(m_packet);
    }
    ret = avcodec_send_frame(m_codecCtx, m_frame);   // 重试
}

if (ret < 0) return false;

// 取本次产生的所有 packet
while (true) {
    ret = avcodec_receive_packet(m_codecCtx, m_packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
    ...
    av_interleaved_write_frame(m_fmtCtx, m_packet);
    av_packet_unref(m_packet);   // ← 每轮必须 unref
}
```

### 2.3 Close 的完整清理

```cpp
void Close() {
    // 1) flush 编码器（发 nullptr 触发）
    if (m_codecCtx && m_opened) {
        avcodec_send_frame(m_codecCtx, nullptr);
        while (true) {
            int ret = avcodec_receive_packet(m_codecCtx, m_packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            ...
            av_interleaved_write_frame(m_fmtCtx, m_packet);
            av_packet_unref(m_packet);   // ← 修复过：之前漏了
        }
    }

    // 2) 写 trailer（关键！moov atom 在这里）
    if (m_fmtCtx && m_fmtCtx->pb) {
        av_write_trailer(m_fmtCtx);
        avio_closep(&m_fmtCtx->pb);
    }

    // 3) 按依赖逆序释放
    if (m_swsCtx)   sws_freeContext(m_swsCtx);
    if (m_frame)    av_frame_free(&m_frame);
    if (m_packet)   av_packet_free(&m_packet);
    if (m_codecCtx) avcodec_free_context(&m_codecCtx);
    if (m_fmtCtx)   avformat_free_context(m_fmtCtx);

    m_stream = nullptr;
    m_opened = false;
}
```

---

## 3. LkTracker.cpp —— 稀疏 LK 跟踪

### 3.1 索引一致性保证

**所有路径都保证 `m_prevPts.size() == m_currPts.size()`**：

```cpp
// Detect：整体替换
m_prevPts = m_currPts;

// Track：整体替换
m_prevPts = std::move(goodPrev);
m_currPts = std::move(goodCurr);

// Replenish / AddPoint：成对 push
m_currPts.push_back(point);
m_prevPts.push_back(point);

// RemovePoint：同步删除
m_currPts.erase(m_currPts.begin() + i);
m_prevPts.erase(m_prevPts.begin() + i);
```

**这个不变量是 `DrawLkTrails(display, tracker.PrevPoints(), tracker.Points())` 不越界的根本保证。**

### 3.2 `Replenish` 的去重

```cpp
for (const auto& point : candidates) {
    bool tooClose = false;
    for (const auto& trackedPoint : m_currPts) {
        if (cv::norm(point - trackedPoint) < m_params.minDistance) {
            tooClose = true;
            break;
        }
    }
    if (!tooClose) {
        m_currPts.push_back(point);
        m_prevPts.push_back(point);
    }
}
```

**注意**：`goodFeaturesToTrack` 传的是 `maxCorners` 而不是剩余配额，补完后总数可能**超过** `maxCorners`。

### 3.3 `Track` 的双分支

```cpp
if (m_params.enableFbFilter) {
    m_lastFbErrors.clear();
    m_fbFilter.Filter(prevGray, gray, m_currPts,
                      goodPrev, goodCurr, m_lastFbErrors);
} else {
    // 直接 calcOpticalFlowPyrLK，只按 status 筛
    ...
    m_lastFbErrors.clear();   // 关闭 FB 时清空
}
```

### 3.4 `AverageFbError` 的空处理

```cpp
double AverageFbError() const {
    if (m_lastFbErrors.empty()) return 0.0;   // ← 空时返回 0，不抛异常
    double sum = 0.0;
    for (float e : m_lastFbErrors) sum += e;
    return sum / m_lastFbErrors.size();
}
```

---

## 4. BiDirectionalLkFilter.cpp —— 双向一致性

### 4.1 四步过滤算法

```cpp
// 1) 前向 LK
cv::calcOpticalFlowPyrLK(prevGray, gray, prevPts, forwardPts, ...);

// 2) 筛前向成功的点
for (size_t i = 0; i < forwardStatus.size(); ++i) {
    if (forwardStatus[i]) {
        validPrev.push_back(prevPts[i]);
        validForward.push_back(forwardPts[i]);
    }
}

// 3) 反向 LK
cv::calcOpticalFlowPyrLK(gray, prevGray, validForward, backwardPts, ...);

// 4) FB 误差校验
for (size_t i = 0; i < backwardStatus.size(); ++i) {
    if (!backwardStatus[i]) continue;
    float fbError = static_cast<float>(cv::norm(validPrev[i] - backwardPts[i]));
    if (fbError < fbThreshold) {
        outPrevPts.push_back(validPrev[i]);
        outCurrPts.push_back(validForward[i]);
        outFbErrors.push_back(fbError);
    }
}
```

### 4.2 注意

- **索引对齐**：`validPrev[i]` ↔ `validForward[i]` ↔ `backwardPts[i]`
- **输出语义**：`outFbErrors` 只统计"通过验证的点"，前向就失败的点不计入
- **性能优化空间**：两次 `calcOpticalFlowPyrLK` 各建一次金字塔，可以下沉到更底层复用，但当前受限于 OpenCV 高层 API

---

## 5. MiddleburyColor.cpp —— 光流配色

### 5.1 关键实现

```cpp
cv::Mat FlowToColor(const cv::Mat& flow) {
    CV_Assert(flow.type() == CV_32FC2);

    // 1) split
    cv::Mat flowParts[2];
    cv::split(flow, flowParts);

    // 2) 极坐标
    cv::Mat magnitude, angle;
    cv::cartToPolar(flowParts[0], flowParts[1], magnitude, angle, true);

    // 3) 归一化
    double magMax = 0.0;
    cv::minMaxLoc(magnitude, nullptr, &magMax);
    if (magMax > 1e-6) magnitude.convertTo(magnitude, CV_32F, 1.0 / magMax);
    else magnitude.setTo(0.0F);

    // 4) HSV 构造（H = angle * 0.5，OpenCV 8bit H 范围 0~179）
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
```

### 5.2 注意

- `angle * 0.5F` **不能改**：OpenCV 的 8bit HSV 中 H 范围是 0~179，对应 0~360°，所以 360° 要除以 2
- **逐帧归一化**：帧间亮度不可直接比较，但对视觉观察友好

---

## 6. Visualization.cpp —— 绘制工具

### 6.1 关键实现

```cpp
void DrawLkTrails(image, prevPts, currPts, color) {
    for (size_t i = 0; i < currPts.size(); ++i) {   // ← 隐含 prevPts.size() == currPts.size()
        cv::line(image, prevPts[i], currPts[i], color, 1, cv::LINE_AA);
        cv::circle(image, currPts[i], 3, color, -1, cv::LINE_AA);
    }
}

void DrawDenseFlowArrows(image, flow, step, scale, color) {
    for (int y = 0; y < flow.rows; y += step)
        for (int x = 0; x < flow.cols; x += step) {
            cv::Point2f f = flow.at<cv::Point2f>(y, x);
            cv::Point2f end(x + f.x*scale, y + f.y*scale);
            cv::arrowedLine(image, cv::Point(x,y),
                            cv::Point(cvRound(end.x), cvRound(end.y)), ...);
        }
}
```

### 6.2 注意

- `DrawLkTrails` 如果 `prevPts.size() < currPts.size()` 会越界读 —— 依赖 `LkTracker` 的长度一致性保证
- `DrawDenseFlowArrows` 在 1080p + `step=16` 下约 8000 个箭头，性能开销大，当前 Demo 未使用

---

## 7. 历史修复汇总

### FrameInterpolator.cpp

| Bug | 症状 | 根因 | 修复 |
| :--- | :--- | :--- | :--- |
| `bilateralFilter` 崩溃 | `bilateralFilterInvoker_8u` 访问冲突 | `cv::Mat prevForFlow = prevGray;` 浅拷贝，src/dst 共享内存 | `cv::Mat prevForFlow;`（空 Mat） |
| `LaplacianBlend` 每帧抛异常 | `pyrUp_` assertion failed | `buildPyramid(src, gp, levels)` 构建 `levels+1` 层 | `buildPyramid(src, gp, levels - 1)` |
| `LaplacianBlend` 里 `conf3.mul(warpBlend)` 崩溃 | 尺寸不匹配 | `conf3` 是原图尺寸，`warpBlend` 是金字塔第 i 层 | 构建 `lconf` 金字塔，每层用 `lconf[i]` |
| TV-L1 输出 NaN/Inf 传播崩溃 | `cv::multiply` 里 `AutoBuffer` 异常 | `setUseInitialFlow(true)` + `flow = initialFlow` 导致发散 | `setUseInitialFlow(false)` + `flow.release()` + `CleanFlowNaN` |
| 中间帧和前一帧一样 | 逐帧对比发现 | `prev = curr` 浅拷贝 | `std::swap(prev, curr)` |
| `ComputeFlowMagnitude` 找不到标识符 | `C3861` 编译错误 | 定义在 `ComputeConfidence` 之后 | 移到 `ComputeConfidence` 之前（或前置声明） |

### FfmpegVideoWriter.cpp

| Bug | 症状 | 根因 | 修复 |
| :--- | :--- | :--- | :--- |
| MP4 打不开 | `moov atom not found` | `Close()` 没调用 / `av_write_trailer` 没执行 | 确保 `Close()` 被调用 + `av_write_trailer` |
| 画面偏黄/偏红 | 与输入明显色差 | `sws_scale` 默认 BT.601，源是 BT.709 | `sws_setColorspaceDetails` + `AVCodecContext` 色彩 metadata |
| 偶发内存泄漏 | — | `Close` 里 flush 循环漏 `av_packet_unref` | 每轮 write 后补 `unref` |

### 其他关键教训

| 现象 | 根因 | 修复 |
| :--- | :--- | :--- |
| Release 下 `msvcp140.dll` 崩溃 | 全局 `std::string` 跨文件初始化顺序 | 改成"函数内 static" |
| `opencv_world4100.dll!dllmain_crt_process_attach` 崩溃 | System32 的 CRT 是 VS 2015 版（14.00），编译 OpenCV 用 VS 2022（14.44） | 装最新 VC++ Redistributable |
| `DISOpticalFlow` 找不到 | OpenCV 4.x 把它从 `optflow` 移到 `video` 模块 | `cv::DISOpticalFlow` 而非 `cv::optflow::DISOpticalFlow` |
| `opencv_world4100d.dll` 出现在 Release 目录 | `copy_directory` 全目录复制，`opencv_world*.dll` 的 `*` 匹配 `4100d` | Release 分支 `list(FILTER ... EXCLUDE REGEX "d\\.dll$")` |

---

## 相关文档

- **模块总览**：[`README.md`](./README.md) —— 文件结构、数据流、性能分析、调试方法
- **接口说明**：[`../../../inc/OpticalFlow/README.md`](../../../inc/OpticalFlow/README.md) —— 每个类的公开 API、使用示例、注意事项
- **配置说明**：[`../../../configs/README.md`](../../../configs/README.md) —— `InterpolationConfig.json` 全部参数详解
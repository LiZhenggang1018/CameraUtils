#ifndef TESTS_OPTICALFLOWTEST_H_
#define TESTS_OPTICALFLOWTEST_H_

#include "OpenCVCommon.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>

#include "DenseFlow.h"
#include "DenseFlowDis.h"
#include "DenseFlowDeepFlow.h"
#include "LkTracker.h"
#include "MiddleburyColor.h"
#include "Visualization.h"
#include "CommonEnv.h"
#include "FfmpegVideoWriter.h"
#include "FrameInterpolator.h"          // ← 新增：显式包含，避免依赖传递
#include "InterpolationConfig.h"        // ← 新增：配置结构体 + 序列化


// ============================================================
// 路径常量：函数内 static，避免跨文件静态初始化顺序问题
// ============================================================
inline const std::string& OpticalFlowDataPathImpl() {
    static const std::string s = EnvRootPath() + "/data/OpticalFlow/1.mp4";
    return s;
}

enum class Mode {
    Lk,
    Dense,
    Dis,
    DeepFlow,
    Overlay
};

inline const char* ModeName(Mode mode) {
    switch (mode) {
        case Mode::Lk:       return "LK";
        case Mode::Dense:    return "Dense";
        case Mode::Dis:      return "Dis";
        case Mode::DeepFlow: return "DeepFlow";
        case Mode::Overlay:  return "Overlay";
    }
    return "Unknown";
}

struct AppState {
    LkTracker* m_tracker = nullptr;
    bool m_paused = false;
    Mode m_mode = Mode::Lk;
    cv::Mat m_currentFrame;
    bool m_deepFlowDirty = true;
};

inline void OnMouse(int event, int x, int y, int /*flags*/, void* userdata) {
    auto* state = static_cast<AppState*>(userdata);
    if (state == nullptr || state->m_tracker == nullptr) {
        return;
    }

    if (event == cv::EVENT_LBUTTONDOWN) {
        state->m_tracker->AddPoint(
            cv::Point2f(static_cast<float>(x), static_cast<float>(y)));
    } else if (event == cv::EVENT_RBUTTONDOWN) {
        state->m_tracker->RemovePoint(
            cv::Point2f(static_cast<float>(x), static_cast<float>(y)),
            8.0);
    }
}

inline int OpticalFlowTest() {
    std::string videoPath = OpticalFlowDataPathImpl();

    cv::VideoCapture capture(videoPath);
    if (!capture.isOpened()) {
        std::cerr << "Cannot open video: " << videoPath << std::endl;
        return -1;
    }

    LkTracker tracker;
    DenseFlowFarneback denseFlowFarneback;
    DenseFlowDis denseFlowDis;
    DenseFlowDeepFlow denseFlowDeepFlow;

    AppState state;
    state.m_tracker = &tracker;

    const std::string windowName = "Optical Flow Demo";
    cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(windowName, OnMouse, &state);

    cv::Mat frame;
    cv::Mat gray;
    cv::Mat prevGray;
    cv::Mat flow;
    cv::Mat denseColor;

    bool firstFrame = true;

    double lastDenseMs = 0.0;
    double lastDeepFlowMs = 0.0;

    using Clock = std::chrono::steady_clock;

    while (true) {
        if (!state.m_paused) {
            if (!capture.read(frame)) {
                break;
            }

            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

            if (firstFrame) {
                tracker.Detect(gray);
                firstFrame = false;
            } else {
                tracker.Track(prevGray, gray);

                if (state.m_mode == Mode::Dense ||
                    state.m_mode == Mode::Dis ||
                    state.m_mode == Mode::Overlay) {

                    const auto t0 = Clock::now();

                    if (state.m_mode == Mode::Dis) {
                        flow = denseFlowDis.Compute(prevGray, gray);
                    } else {
                        flow = denseFlowFarneback.Compute(prevGray, gray);
                    }

                    const auto t1 = Clock::now();
                    lastDenseMs =
                        std::chrono::duration<double, std::milli>(t1 - t0).count();
                    denseColor = FlowToColor(flow);
                }
            }

            prevGray = gray.clone();
            state.m_currentFrame = frame.clone();
            state.m_deepFlowDirty = true;
        }

        if (state.m_currentFrame.empty()) {
            continue;
        }

        if (state.m_mode == Mode::DeepFlow &&
            state.m_paused &&
            state.m_deepFlowDirty &&
            !prevGray.empty() &&
            !gray.empty()) {

            const auto t0 = Clock::now();
            flow = denseFlowDeepFlow.Compute(prevGray, gray);
            const auto t1 = Clock::now();
            lastDeepFlowMs =
                std::chrono::duration<double, std::milli>(t1 - t0).count();
            denseColor = FlowToColor(flow);
            state.m_deepFlowDirty = false;
        }

        cv::Mat display;

        switch (state.m_mode) {
            case Mode::Lk: {
                display = state.m_currentFrame.clone();
                DrawLkTrails(display, tracker.PrevPoints(), tracker.Points());
                break;
            }
            case Mode::Dense:
            case Mode::Dis: {
                display = denseColor.empty()
                              ? state.m_currentFrame.clone()
                              : denseColor.clone();
                break;
            }
            case Mode::DeepFlow: {
                display = denseColor.empty()
                              ? state.m_currentFrame.clone()
                              : denseColor.clone();
                break;
            }
            case Mode::Overlay: {
                display = denseColor.empty()
                              ? state.m_currentFrame.clone()
                              : denseColor.clone();
                DrawLkTrails(display, tracker.PrevPoints(), tracker.Points());
                break;
            }
        }

        char statusBuffer[256];

        if (state.m_mode == Mode::Lk || state.m_mode == Mode::Overlay) {
            char fbInfo[64];
            if (tracker.IsFbFilterEnabled()) {
                std::snprintf(
                    fbInfo, sizeof(fbInfo),
                    "FB=%.2f/%.2f",
                    tracker.FbThreshold(),
                    tracker.AverageFbError());
            } else {
                std::snprintf(fbInfo, sizeof(fbInfo), "FB=OFF");
            }

            if (state.m_mode == Mode::Lk) {
                std::snprintf(
                    statusBuffer, sizeof(statusBuffer),
                    "Mode=LK maxLv=%d trk=%d %s %.1fms %s",
                    tracker.MaxLevel(),
                    tracker.TrackedCount(),
                    fbInfo,
                    lastDenseMs,
                    state.m_paused ? "PAUSED" : "PLAYING");
            } else {
                std::snprintf(
                    statusBuffer, sizeof(statusBuffer),
                    "Mode=Overlay win=%d trk=%d %s %.1fms %s",
                    denseFlowFarneback.Winsize(),
                    tracker.TrackedCount(),
                    fbInfo,
                    lastDenseMs,
                    state.m_paused ? "PAUSED" : "PLAYING");
            }
        } else if (state.m_mode == Mode::Dense) {
            std::snprintf(
                statusBuffer, sizeof(statusBuffer),
                "Mode=Dense win=%d trk=%d %.1fms %s",
                denseFlowFarneback.Winsize(),
                tracker.TrackedCount(),
                lastDenseMs,
                state.m_paused ? "PAUSED" : "PLAYING");
        } else if (state.m_mode == Mode::Dis) {
            std::snprintf(
                statusBuffer, sizeof(statusBuffer),
                "Mode=Dis preset=%s %.1fms %s",
                denseFlowDis.PresetName(),
                lastDenseMs,
                state.m_paused ? "PAUSED" : "PLAYING");
        } else {
            if (state.m_paused) {
                std::snprintf(
                    statusBuffer, sizeof(statusBuffer),
                    "Mode=DeepFlow %.1fms PAUSED",
                    lastDeepFlowMs);
            } else {
                std::snprintf(
                    statusBuffer, sizeof(statusBuffer),
                    "Mode=DeepFlow -- pause to compute -- PLAYING");
            }
        }

        cv::putText(
            display,
            statusBuffer,
            cv::Point(10, 30),
            cv::FONT_HERSHEY_SIMPLEX,
            0.5,
            cv::Scalar(0, 255, 0),
            1,
            cv::LINE_AA);

        cv::putText(
            display,
            "m=mode space=pause r=reset q=quit f=fb b/B=fbThr +/-=maxLv w/W=win p=preset",
            cv::Point(10, display.rows - 10),
            cv::FONT_HERSHEY_SIMPLEX,
            0.45,
            cv::Scalar(200, 200, 200),
            1,
            cv::LINE_AA);

        cv::imshow(windowName, display);

        const int key = cv::waitKey(state.m_paused ? 30 : 1);

        if (key == 27 || key == 'q') {
            break;
        }

        if (key == ' ') {
            state.m_paused = !state.m_paused;
        } else if (key == 'm') {
            const int nextMode =
                (static_cast<int>(state.m_mode) + 1) % 5;
            state.m_mode = static_cast<Mode>(nextMode);
            if (state.m_mode == Mode::DeepFlow) {
                state.m_deepFlowDirty = true;
            }
        } else if (key == '+' || key == '=') {
            tracker.SetMaxLevel(std::min(tracker.MaxLevel() + 1, 6));
        } else if (key == '-') {
            tracker.SetMaxLevel(std::max(tracker.MaxLevel() - 1, 0));
        } else if (key == 'w') {
            denseFlowFarneback.SetWinsize(
                std::min(denseFlowFarneback.Winsize() + 2, 51));
        } else if (key == 'W') {
            denseFlowFarneback.SetWinsize(
                std::max(denseFlowFarneback.Winsize() - 2, 5));
        } else if (key == 'b') {
            tracker.SetFbThreshold(
                std::min(tracker.FbThreshold() + 0.25, 5.0));
        } else if (key == 'B') {
            tracker.SetFbThreshold(
                std::max(tracker.FbThreshold() - 0.25, 0.1));
        } else if (key == 'f') {
            tracker.SetEnableFbFilter(!tracker.IsFbFilterEnabled());
        } else if (key == 'p') {
            denseFlowDis.CyclePreset();
        } else if (key == 'r') {
            tracker.Reset();
            firstFrame = true;
            flow.release();
            denseColor.release();
            state.m_deepFlowDirty = true;
            lastDenseMs = 0.0;
            lastDeepFlowMs = 0.0;
            capture.set(cv::CAP_PROP_POS_FRAMES, 0);
        }
    }

    capture.release();
    cv::destroyAllWindows();
    return 0;
}

// ============================================================
// 通用插帧：任意倍率 + 配置驱动
// 核心优化：每对帧只算一次双向光流，所有中间帧复用
// ============================================================
inline int UpsampleVideo(const std::string& inputPath,
                         const std::string& outputPath,
                         const InterpolationConfig& cfg) {
    std::printf("========== UpsampleVideo begin ==========\n");
    std::printf("[Config] factor=%d, preset=%s, crf=%d, bitrate=%d\n",
                cfg.factor, cfg.preset.c_str(), cfg.crf, cfg.bitrate);
    std::printf("[Input ] %s\n", inputPath.c_str());
    std::printf("[Output] %s\n", outputPath.c_str());

    if (cfg.factor < 2) {
        std::cerr << "[ERROR] factor must be >= 2" << std::endl;
        return -1;
    }

    cv::VideoCapture cap(inputPath);
    if (!cap.isOpened()) {
        std::cerr << "[ERROR] Cannot open input: " << inputPath << std::endl;
        return -1;
    }

    const int    W           = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    const int    H           = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    const double srcFps      = cap.get(cv::CAP_PROP_FPS);
    const long   totalFrames = static_cast<long>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    std::printf("[Meta ] W=%d H=%d srcFps=%.3f totalFrames=%ld\n",
                W, H, srcFps, totalFrames);

    if (W <= 0 || H <= 0 || srcFps <= 0) {
        std::cerr << "[ERROR] Invalid input metadata" << std::endl;
        return -1;
    }

    // 用配置构造 FrameInterpolator::Params
    FrameInterpolator::Params params;
    params.flowBackend          = cfg.flowBackend;
    params.disPreset            = cfg.disPreset;
    params.maxCoarseLevels      = cfg.maxCoarseLevels;
    params.fbThreshold          = cfg.fbThreshold;
    params.enableStaticProtect  = cfg.enableStaticProtect;
    params.staticMagThreshold   = cfg.staticMagThreshold;
    params.confidenceBlurSigma  = cfg.confidenceBlurSigma;
    params.occlusionBlurSigma   = cfg.occlusionBlurSigma;
    params.enableTemporalSmooth = cfg.enableTemporalSmooth;
    params.temporalAlpha        = cfg.temporalAlpha;
    params.temporalMagThreshold = cfg.temporalMagThreshold;
    FrameInterpolator interp(params);

    // 用配置构造 FfmpegVideoWriter::EncoderParams
    FfmpegVideoWriter writer;
    FfmpegVideoWriter::EncoderParams enc;
    enc.preset = cfg.preset;
    enc.crf    = cfg.crf;

    const int outFps = static_cast<int>(srcFps * cfg.factor);
    if (!writer.Open(outputPath, W, H, outFps, cfg.bitrate, enc)) {
        std::cerr << "[ERROR] FfmpegVideoWriter Open failed" << std::endl;
        return -1;
    }
    std::printf("[Writer] opened, encoder=%s, %dx%d @ %dfps\n",
                writer.EncoderName(), W, H, outFps);

    // -------- 读第一帧 --------
    cv::Mat prev;
    const bool firstOk = cap.read(prev);
    std::printf("[Read #0] ret=%d empty=%d size=%dx%d type=%d channels=%d\n",
                static_cast<int>(firstOk), static_cast<int>(prev.empty()),
                prev.cols, prev.rows, prev.type(), prev.channels());

    if (!firstOk || prev.empty()) {
        std::cerr << "[ERROR] Cannot read first frame. "
                  << "VideoCapture backend may not decode this mp4." << std::endl;
        writer.Close();
        return -1;
    }

    if (prev.cols != W || prev.rows != H) {
        std::printf("[WARN ] frame size %dx%d != meta %dx%d\n",
                    prev.cols, prev.rows, W, H);
    }

    // -------- 写第一帧 --------
    if (!writer.WriteFrame(prev)) {
        std::cerr << "[ERROR] WriteFrame(first) returned false" << std::endl;
        writer.Close();
        return -1;
    }
    int readCount  = 1;
    int writeCount = 1;

    // -------- 主循环 --------
    cv::Mat curr;
    while (cap.read(curr)) {
        // 防御：prev 和 curr 绝不能共享内存（cv::Mat 浅拷贝陷阱）
        if (prev.data == curr.data) {
            std::fprintf(stderr, "[BUG] prev and curr share the same memory!\n");
            break;
        }
        if (curr.empty()) {
            std::cerr << "[WARN ] empty frame at read#" << readCount << std::endl;
            continue;
        }
        ++readCount;

        if (prev.size() != curr.size()) {
            std::cerr << "[WARN ] size mismatch at read#" << readCount
                      << ": " << curr.cols << "x" << curr.rows
                      << " vs prev " << prev.cols << "x" << prev.rows << std::endl;
            break;
        }

        // ★ 关键优化：每对帧只算一次双向光流，所有中间帧复用
        cv::Mat flowFwd, flowBwd;
        interp.ComputeFlows(prev, curr, flowFwd, flowBwd);

        // 生成 factor-1 个中间帧：t = 1/factor, 2/factor, ..., (factor-1)/factor
        for (int k = 1; k < cfg.factor; ++k) {
            const double t = static_cast<double>(k) / cfg.factor;
            cv::Mat mid = interp.BlendFromFlows(prev, curr, flowFwd, flowBwd, t);

            if (mid.empty() || mid.cols != prev.cols || mid.rows != prev.rows) {
                std::cerr << "[WARN ] BlendFromFlows bad output at t=" << t
                          << " read#" << readCount << std::endl;
                continue;
            }

            if (!writer.WriteFrame(mid)) {
                std::cerr << "[ERROR] WriteFrame(mid t=" << t << ") failed at read#"
                          << readCount << std::endl;
                writer.Close();
                return -1;
            }
            ++writeCount;
        }

        if (!writer.WriteFrame(curr)) {
            std::cerr << "[ERROR] WriteFrame(curr) failed at read#" << readCount << std::endl;
            break;
        }
        ++writeCount;

        // 必须 swap，不能用 prev = curr（cv::Mat 浅拷贝陷阱）
        std::swap(prev, curr);

        if (readCount % 30 == 0) {
            std::printf("[Progress] read=%d write=%d\n", readCount, writeCount);
        }
    }

    std::printf("[Done] read=%d write=%d\n", readCount, writeCount);
    writer.Close();
    std::printf("========== UpsampleVideo end ==========\n");
    return 0;
}

// ============================================================
// 便捷入口：读配置 + 默认输入/输出路径
// 输出文件按 factor 自动命名：1_60fps.mp4 / 1_90fps.mp4 / 1_120fps.mp4
// ============================================================
inline int Upsample30To60() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    const std::string configPath = InterpolationConfig::DefaultPath();
    const InterpolationConfig cfg = InterpolationConfig::LoadFromFile(configPath);

    const std::string inputPath  = OpticalFlowDataPathImpl();

    // 根据 factor 自动推断输出帧率，命名输出文件
    const int outFps = 30 * cfg.factor;
    const std::string outputPath = EnvRootPath()
                                 + "/data/OpticalFlow/1_"
                                 + std::to_string(outFps) + "fps.mp4";

    return UpsampleVideo(inputPath, outputPath, cfg);
}

#endif // TESTS_OPTICALFLOWTEST_H_
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


inline const std::string OPTICAL_FLOW_DATA_PATH = std::string(PROJECT_ROOT_DIR) + "/data/OpticalFlow/1.mp4";

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
    std::string videoPath = OPTICAL_FLOW_DATA_PATH;

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

    // 计时：记录最近一次稠密光流计算耗时（ms）
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
            // 新帧到达，DeepFlow 需要重算
            state.m_deepFlowDirty = true;
        }

        if (state.m_currentFrame.empty()) {
            continue;
        }

        // DeepFlow 只在暂停时计算一次
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

        // ---- 第一行状态栏 ----
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
                    lastDenseMs,   // LK 模式下此值可能为旧值，保留以显示最近稠密计算耗时
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
        } else { // DeepFlow
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

        // ---- 底部提示栏 ----
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
            // 切到 DeepFlow 时标记需要重算（下一次暂停时触发）
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

inline int Upsample30To60() {
    const std::string inputPath = OPTICAL_FLOW_DATA_PATH;
    const std::string outputPath = std::string(PROJECT_ROOT_DIR) + "/data/OpticalFlow/1_60fps.mp4";
    cv::VideoCapture cap(inputPath);
    if (!cap.isOpened()) {
        std::cerr << "Cannot open input: " << inputPath << std::endl;
        return -1;
    }

    const int W = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    const int H = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    const double srcFps = cap.get(cv::CAP_PROP_FPS);

    FrameInterpolator interp;
    FfmpegVideoWriter writer;
    const int outFps = static_cast<int>(srcFps * 2);
    if (!writer.Open(outputPath, W, H, outFps, 8000000)) {
        std::cerr << "FfmpegVideoWriter Open failed" << std::endl;
        return -1;
    }

    cv::Mat prev;
    cv::Mat curr;
    if (!cap.read(prev)) {
        writer.Close();
        return -1;
    }
    writer.WriteFrame(prev);   // 原始第 1 帧

    while (cap.read(curr)) {
        cv::Mat mid = interp.Interpolate(prev, curr, 0.5);
        writer.WriteFrame(mid);    // 插帧
        writer.WriteFrame(curr);   // 原始后帧
        prev = curr;
    }

    // 必须调用 Close()，否则 MP4 没有 moov atom，PotPlayer/ffprobe 打不开
    writer.Close();
    return 0;
}

#endif // TESTS_OPTICALFLOWTEST_H_

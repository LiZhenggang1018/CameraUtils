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


// ============================================================
// 路径常量：改成"函数内 static"，避免跨文件静态初始化顺序问题
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
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    const std::string inputPath  = OpticalFlowDataPathImpl();
    const std::string outputPath = EnvRootPath() + "/data/OpticalFlow/1_60fps.mp4";

    std::printf("========== Upsample30To60 begin ==========\n");
    std::printf("[Input ] %s\n",  inputPath.c_str());
    std::printf("[Output] %s\n", outputPath.c_str());

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

    FrameInterpolator interp;
    FfmpegVideoWriter writer;
    const int outFps = static_cast<int>(srcFps * 2);
    // 设置码率16Mbps，保证输出视频质量
    // 对于1080P 60FPS,推荐码率：12~20Mbps
    // 对于4K 60FPS,推荐码率：50~80 Mbps

    if (!writer.Open(outputPath, W, H, outFps, 16000000)) {
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
        if (prev.data == curr.data) {
            std::fprintf(stderr, "[BUG] prev and curr share the same memory!\n");
            break;
        }
        if (prev.size() != curr.size()) {
            std::fprintf(stderr, "[BUG] size mismatch\n");
            break;
        }
        //std::printf("[LOOP #%d] prev.data=%p, curr.data=%p\n", readCount, (void*)prev.data, (void*)curr.data);
        if (curr.empty()) {
            std::cerr << "[WARN ] empty frame at read#" << readCount << std::endl;
            continue;
        }
        ++readCount;

        if (curr.cols != prev.cols || curr.rows != prev.rows) {
            std::cerr << "[WARN ] size mismatch at read#" << readCount
                      << ": " << curr.cols << "x" << curr.rows
                      << " vs prev " << prev.cols << "x" << prev.rows << std::endl;
        }

        cv::Mat mid = interp.Interpolate(prev, curr, 0.5);
        if (mid.empty() || mid.cols != curr.cols || mid.rows != curr.rows) {
            std::cerr << "[WARN ] Interpolate returned bad size at read#" << readCount << std::endl;
        }

        if (!writer.WriteFrame(mid)) {
            std::cerr << "[ERROR] WriteFrame(mid) failed at read#" << readCount << std::endl;
            break;
        }
        ++writeCount;

        if (!writer.WriteFrame(curr)) {
            std::cerr << "[ERROR] WriteFrame(curr) failed at read#" << readCount << std::endl;
            break;
        }
        ++writeCount;

        std::swap(prev, curr);

        if (readCount % 30 == 0) {
            std::printf("[Progress] read=%d write=%d\n", readCount, writeCount);
        }
    }

    std::printf("[Done] read=%d write=%d\n", readCount, writeCount);

    writer.Close();
    std::printf("========== Upsample30To60 end ==========\n");
    return 0;
}

#endif // TESTS_OPTICALFLOWTEST_H_
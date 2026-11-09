#ifndef TESTS_OPTICALFLOWTEST_H_
#define TESTS_OPTICALFLOWTEST_H_

#include "OpenCVCommon.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

#include "DenseFlow.h"
#include "LkTracker.h"
#include "MiddleburyColor.h"
#include "Visualization.h"
#include "CommonEnv.h"

inline const std::string OPTICAL_FLOW_DATA_PATH = std::string(PROJECT_ROOT_DIR) + "/data/OpticalFlow/1.mp4";

enum class Mode {
    Lk,
    Dense,
    Overlay
};

const char* ModeName(Mode mode) {
    switch (mode) {
        case Mode::Lk:
            return "LK";
        case Mode::Dense:
            return "Dense";
        case Mode::Overlay:
            return "Overlay";
    }
    return "Unknown";
}

struct AppState {
    LkTracker* m_tracker = nullptr;
    bool m_paused = false;
    Mode m_mode = Mode::Lk;
    cv::Mat m_currentFrame;
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
    DenseFlowFarneback denseFlow;

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
                    state.m_mode == Mode::Overlay) {
                    flow = denseFlow.Compute(prevGray, gray);
                    denseColor = FlowToColor(flow);
                }
            }

            prevGray = gray.clone();
            state.m_currentFrame = frame.clone();
        }

        if (state.m_currentFrame.empty()) {
            continue;
        }

        cv::Mat display;

        if (state.m_mode == Mode::Lk) {
            display = state.m_currentFrame.clone();
            DrawLkTrails(
                display,
                tracker.PrevPoints(),
                tracker.Points());
        } else if (state.m_mode == Mode::Dense) {
            display = denseColor.empty()
                          ? state.m_currentFrame.clone()
                          : denseColor.clone();
        } else {
            display = denseColor.empty()
                          ? state.m_currentFrame.clone()
                          : denseColor.clone();

            DrawLkTrails(
                display,
                tracker.PrevPoints(),
                tracker.Points());
        }

        char statusBuffer[192];
        std::snprintf(
            statusBuffer,
            sizeof(statusBuffer),
            "Mode=%s [m]  maxLevel=%d [+/-]  winsize=%d [w/W]  tracked=%d  %s",
            ModeName(state.m_mode),
            tracker.MaxLevel(),
            denseFlow.Winsize(),
            tracker.TrackedCount(),
            state.m_paused ? "PAUSED" : "PLAYING");

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
            "space=pause  r=reset  q=quit",
            cv::Point(10, display.rows - 10),
            cv::FONT_HERSHEY_SIMPLEX,
            0.5,
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
                (static_cast<int>(state.m_mode) + 1) % 3;
            state.m_mode = static_cast<Mode>(nextMode);
        } else if (key == '+' || key == '=') {
            tracker.SetMaxLevel(std::min(tracker.MaxLevel() + 1, 6));
        } else if (key == '-') {
            tracker.SetMaxLevel(std::max(tracker.MaxLevel() - 1, 0));
        } else if (key == 'w') {
            denseFlow.SetWinsize(
                std::min(denseFlow.Winsize() + 2, 51));
        } else if (key == 'W') {
            denseFlow.SetWinsize(
                std::max(denseFlow.Winsize() - 2, 5));
        } else if (key == 'r') {
            tracker.Reset();
            firstFrame = true;
            flow.release();
            denseColor.release();
            capture.set(cv::CAP_PROP_POS_FRAMES, 0);
        }
    }

    capture.release();
    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_OPTICALFLOWTEST_H_

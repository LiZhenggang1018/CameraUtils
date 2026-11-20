#ifndef TESTS_CLI_COMMANDTABLE_H
#define TESTS_CLI_COMMANDTABLE_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "CoreUtils.h"

#include "Alignment/BlendingDemo.h"
#include "Alignment/BlendingTest.h"
#include "Alignment/ExposureCompensationDemo.h"
#include "Alignment/ExposureCompensationTest.h"
#include "Alignment/HomographyDemo.h"
#include "Alignment/HomographyTest.h"
#include "Alignment/PanoramaDemo.h"
#include "Alignment/PanoramaTest.h"
#include "Alignment/VignettingDemo.h"
#include "Alignment/VignettingTest.h"
#include "Exiv2Test.h"
#include "FFMpegTest.h"
#include "FeatureMatching/FeatureMatcherDemo.h"
#include "FeatureMatching/FeatureMatcherTest.h"
#include "FeatureMatching/KeypointDetectorDemo.h"
#include "FeatureMatching/KeypointDetectorTest.h"
#include "FeatureMatching/LocalFeatureDemo.h"
#include "FeatureMatching/LocalFeatureTest.h"
#include "HdrMerge/HdrMergeTest.h"
#include "ImageProcessing/FourierDemo.h"
#include "ImageProcessing/LinearFilterDemo.h"
#include "ImageProcessing/LinearFilterTest.h"
#include "ImageProcessing/NonLinearFilterDemo.h"
#include "ImageProcessing/NonLinearFilterTest.h"
#include "ImageProcessing/PyramidDemo.h"
#include "ImageProcessing/PyramidTest.h"
#include "ImageProcessing/SobelTest.h"
#include "LivePhotoSmoothPlay/LivePhotoSmoothPlayTest.h"
#include "MagicEnumTest.h"
#include "OpenCVTest.h"
#include "OpticalFlow/OpticalFlowTest.h"
#include "TestCase.h"
#include "Type2InstMapTest.h"

namespace Cli {

/// 命令分类：决定是否依赖外部资源
enum class Label : std::uint8_t {
    UNIT,  ///< 纯内存运算，无外部依赖
    MEDIA, ///< 需要 data/ 目录下的媒体素材
    GUI    ///< 需要图形界面（imshow / waitKey）
};

struct Command {
    std::string name;
    std::string description;
    Label label;
    std::function<int()> run;
};

inline const char* LabelName(Label label)
{
    switch (label) {
    case Label::UNIT:
        return "unit";
    case Label::MEDIA:
        return "media";
    case Label::GUI:
        return "gui";
    }
    return "unknown";
}

/// data/ 目录是否可用（媒体类命令的前置条件）
inline bool MediaDataAvailable()
{
    const std::string dataDir = EnvRootPath() + "/data";
    return std::filesystem::is_directory(dataDir);
}

inline const std::vector<Command>& AllCommands()
{
    static const std::vector<Command> commands = {
        // ---------- 纯工具层 ----------
        {.name = "deque-blocking",
         .description = "BlockingDeque 阻塞模式",
         .label = Label::UNIT,
         .run =
             [] {
                 TestInBlockingMode();
                 return 0;
             }},
        {.name = "deque-nonblocking",
         .description = "BlockingDeque 非阻塞模式",
         .label = Label::UNIT,
         .run =
             [] {
                 TestInNonBlockingMode();
                 return 0;
             }},
        {.name = "find-then-or-else",
         .description = "Find/AndThen/OrElse",
         .label = Label::UNIT,
         .run =
             [] {
                 TestFindAndThenOrElse();
                 return 0;
             }},
        {.name = "overload-ref",
         .description = "Overload 引用重载",
         .label = Label::UNIT,
         .run =
             [] {
                 TestOverloadRef();
                 return 0;
             }},
        {.name = "timeout-monitor",
         .description = "TimeoutMonitor",
         .label = Label::UNIT,
         .run =
             [] {
                 TimeoutMonitorTest();
                 return 0;
             }},
        {.name = "misc",
         .description = "Misc：Enum2Str / Scoped / InstanceCounter",
         .label = Label::UNIT,
         .run =
             [] {
                 MiscTest();
                 return 0;
             }},
        {.name = "circular-queue",
         .description = "CircularQueue",
         .label = Label::UNIT,
         .run =
             [] {
                 TestCircularQueue();
                 return 0;
             }},
        {.name = "overwrite-function",
         .description = "OverwriteFunction",
         .label = Label::UNIT,
         .run =
             [] {
                 TestOverwriteFunction();
                 return 0;
             }},
        {.name = "msg-queue",
         .description = "MsgQueue 任务队列",
         .label = Label::UNIT,
         .run =
             [] {
                 MsgQueueTest();
                 return 0;
             }},
        {.name = "event",
         .description = "Event/EventDispatcher",
         .label = Label::UNIT,
         .run =
             [] {
                 TestEvent();
                 return 0;
             }},
        {.name = "factory-json",
         .description = "GeneralFactory + Json 配置",
         .label = Label::UNIT,
         .run =
             [] {
                 FactoryJsonTest();
                 return 0;
             }},
        {.name = "sync-agency",
         .description = "SyncAgency 跨模块同步",
         .label = Label::UNIT,
         .run =
             [] {
                 TestSyncAgency();
                 return 0;
             }},
        {.name = "thread-pool",
         .description = "ThreadPool",
         .label = Label::UNIT,
         .run =
             [] {
                 ThreadPoolTest();
                 return 0;
             }},
        {.name = "instance-counter",
         .description = "InstanceCounter",
         .label = Label::UNIT,
         .run =
             [] {
                 InstanceCounterTest();
                 return 0;
             }},
        {.name = "type2instmap",
         .description = "Type2InstMap",
         .label = Label::UNIT,
         .run =
             [] {
                 Type2InstMapTest();
                 return 0;
             }},
        {.name = "magic-enum",
         .description = "MagicEnum 枚举转换",
         .label = Label::UNIT,
         .run =
             [] {
                 MagicEnumTest::TestMagicEnum();
                 return 0;
             }},
        {.name = "sobel",
         .description = "Sobel 边缘检测（手写实现 + 真值断言）",
         .label = Label::UNIT,
         .run =
             [] {
                 SobelEdgeTest();
                 return 0;
             }},
        {.name = "linear-filter",
         .description = "线性滤波（高斯/均值/锐化/Sobel）",
         .label = Label::UNIT,
         .run =
             [] {
                 LinearFilterTest();
                 return 0;
             }},
        {.name = "nonlinear-filter",
         .description = "非线性滤波（中值/双边）",
         .label = Label::UNIT,
         .run =
             [] {
                 NonLinearFilterTest();
                 return 0;
             }},
        {.name = "pyramid",
         .description = "金字塔（高斯/拉普拉斯）",
         .label = Label::UNIT,
         .run =
             [] {
                 PyramidTest();
                 return 0;
             }},
        {.name = "keypoint-detect",
         .description = "关键点检测（Harris/Shi-Tomasi/FAST）",
         .label = Label::UNIT,
         .run =
             [] {
                 KeypointDetectorTest();
                 return 0;
             }},
        {.name = "local-feature",
         .description = "局部特征（SIFT/ORB/AKAZE）",
         .label = Label::UNIT,
         .run =
             [] {
                 LocalFeatureTest();
                 return 0;
             }},
        {.name = "feature-match",
         .description = "特征匹配 + RANSAC（BF/FLANN/ratio/RANSAC 单应）",
         .label = Label::UNIT,
         .run =
             [] {
                 FeatureMatcherTest();
                 return 0;
             }},
        {.name = "homography",
         .description = "单应与 warping（配准 + warpPerspective）",
         .label = Label::UNIT,
         .run =
             [] {
                 HomographyTest();
                 return 0;
             }},
        {.name = "blending",
         .description = "图像融合（多波段 blending）",
         .label = Label::UNIT,
         .run =
             [] {
                 BlendingTest();
                 return 0;
             }},
        {.name = "exposure-comp",
         .description = "曝光补偿（增益估计 + 亮度对齐）",
         .label = Label::UNIT,
         .run =
             [] {
                 ExposureCompensationTest();
                 return 0;
             }},
        {.name = "vignetting",
         .description = "渐晕校正（径向衰减模型）",
         .label = Label::UNIT,
         .run =
             [] {
                 VignettingTest();
                 return 0;
             }},
        {.name = "panorama",
         .description = "多图全景拼接",
         .label = Label::UNIT,
         .run =
             [] {
                 PanoramaTest();
                 return 0;
             }},

#if VAS_HAS_STDEXEC
        {.name = "vas-concurrency",
         .description = "stdexec 并发管线",
         .label = Label::UNIT,
         .run =
             [] {
                 VasConcurrencyTest();
                 return 0;
             }},
#endif

        // ---------- OpenCV / FFmpeg / exiv2 ----------
        {.name = "exif",
         .description = "Exiv2 读取 EXIF",
         .label = Label::UNIT,
         .run =
             [] {
                 CameraUtils::Exif2Test();
                 return 0;
             }},

        // ---------- 依赖媒体素材 ----------
        {.name = "ffmpeg",
         .description = "FFmpeg 基础解码",
         .label = Label::MEDIA,
         .run =
             [] {
                 TestFFMpeg();
                 return 0;
             }},
        {.name = "optical-flow-upsample",
         .description = "光流 30→60 帧插值",
         .label = Label::MEDIA,
         .run = [] { return Upsample30To60(); }},
        {.name = "hdr-merge",
         .description = "HDR 融合",
         .label = Label::MEDIA,
         .run =
             [] {
                 RunHdrMergeTest();
                 return 0;
             }},
        {.name = "hdr-merge-multilevel",
         .description = "HDR 融合（多档位）",
         .label = Label::MEDIA,
         .run =
             [] {
                 RunHdrMergeMultiLevelTest();
                 return 0;
             }},
        {.name = "hdr-merge-physical",
         .description = "HDR 物理融合",
         .label = Label::MEDIA,
         .run =
             [] {
                 RunHdrMergePhysicalTest();
                 return 0;
             }},
        {.name = "hdr-merge-ghost",
         .description = "HDR 鬼影抑制",
         .label = Label::MEDIA,
         .run =
             [] {
                 RunHdrMergeGhostSuppressionTest();
                 return 0;
             }},
        {.name = "livephoto",
         .description = "LivePhoto 平滑播放",
         .label = Label::MEDIA,
         .run =
             [] {
                 RunLivePhotoSmoothPlayTest();
                 return 0;
             }},
        {.name = "livephoto-reg",
         .description = "LivePhoto 图像配准",
         .label = Label::MEDIA,
         .run =
             [] {
                 RegTest();
                 return 0;
             }},

        // ---------- 需要图形界面 ----------
        {.name = "opencv-basic",
         .description = "OpenCV 基础读写与显示",
         .label = Label::GUI,
         .run =
             [] {
                 OpenCVTest();
                 return 0;
             }},
        {.name = "optical-flow",
         .description = "光流可视化（交互窗口）",
         .label = Label::GUI,
         .run = [] { return OpticalFlowTest(); }},
        {.name = "linear-filter-demo",
         .description = "线性滤波调参演示（交互窗口）",
         .label = Label::GUI,
         .run = [] { return LinearFilterDemo(); }},
        {.name = "nonlinear-filter-demo",
         .description = "非线性滤波调参演示（交互窗口）",
         .label = Label::GUI,
         .run = [] { return NonLinearFilterDemo(); }},
        {.name = "pyramid-demo",
         .description = "金字塔演示（高斯/拉普拉斯）",
         .label = Label::GUI,
         .run = [] { return PyramidDemo(); }},
        {.name = "fourier-demo",
         .description = "傅里叶变换频谱与高低频滤波演示",
         .label = Label::GUI,
         .run = [] { return FourierDemo(); }},
        {.name = "keypoint-detect-demo",
         .description = "关键点检测演示（Harris/Shi-Tomasi/FAST）",
         .label = Label::GUI,
         .run = [] { return KeypointDetectorDemo(); }},
        {.name = "local-feature-demo",
         .description = "局部特征演示（SIFT/ORB/AKAZE）",
         .label = Label::GUI,
         .run = [] { return LocalFeatureDemo(); }},
        {.name = "feature-match-demo",
         .description = "特征匹配 + RANSAC 演示",
         .label = Label::GUI,
         .run = [] { return FeatureMatcherDemo(); }},
        {.name = "homography-demo",
         .description = "单应 + warping 演示",
         .label = Label::GUI,
         .run = [] { return HomographyDemo(); }},
        {.name = "blending-demo",
         .description = "图像融合（多波段 blending）演示",
         .label = Label::GUI,
         .run = [] { return BlendingDemo(); }},
        {.name = "exposure-comp-demo",
         .description = "曝光补偿演示",
         .label = Label::GUI,
         .run = [] { return ExposureCompensationDemo(); }},
        {.name = "vignetting-demo",
         .description = "渐晕校正演示",
         .label = Label::GUI,
         .run = [] { return VignettingDemo(); }},
        {.name = "panorama-demo",
         .description = "多图全景拼接演示",
         .label = Label::GUI,
         .run = [] { return PanoramaDemo(); }},
    };
    return commands;
}

} // namespace Cli

#endif // TESTS_CLI_COMMANDTABLE_H

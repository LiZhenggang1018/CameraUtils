#ifndef TESTS_CLI_COMMANDTABLE_H
#define TESTS_CLI_COMMANDTABLE_H

#include <functional>
#include <string>
#include <vector>

#include "CoreUtils.h"

#include "Exiv2Test.h"
#include "FFMpegTest.h"
#include "HarrisDetect.h"
#include "HdrMerge/HdrMergeTest.h"
#include "LivePhotoSmoothPlay/LivePhotoSmoothPlayTest.h"
#include "MagicEnumTest.h"
#include "OpenCVTest.h"
#include "OpticalFlowTest.h"
#include "TestCase.h"
#include "Type2InstMapTest.h"

namespace Cli {

/// 命令分类：决定是否依赖外部资源
enum class Label {
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
        {"deque-blocking", "BlockingDeque 阻塞模式", Label::UNIT,
         [] {
             TestInBlockingMode();
             return 0;
         }},
        {"deque-nonblocking", "BlockingDeque 非阻塞模式", Label::UNIT,
         [] {
             TestInNonBlockingMode();
             return 0;
         }},
        {"find-then-or-else", "Find/AndThen/OrElse", Label::UNIT,
         [] {
             TestFindAndThenOrElse();
             return 0;
         }},
        {"overload-ref", "Overload 引用重载", Label::UNIT,
         [] {
             TestOverloadRef();
             return 0;
         }},
        {"timeout-monitor", "TimeoutMonitor", Label::UNIT,
         [] {
             TimeoutMonitorTest();
             return 0;
         }},
        {"misc", "Misc：Enum2Str / Scoped / InstanceCounter", Label::UNIT,
         [] {
             MiscTest();
             return 0;
         }},
        {"circular-queue", "CircularQueue", Label::UNIT,
         [] {
             TestCircularQueue();
             return 0;
         }},
        {"overwrite-function", "OverwriteFunction", Label::UNIT,
         [] {
             TestOverwriteFunction();
             return 0;
         }},
        {"msg-queue", "MsgQueue 任务队列", Label::UNIT,
         [] {
             MsgQueueTest();
             return 0;
         }},
        {"event", "Event/EventDispatcher", Label::UNIT,
         [] {
             TestEvent();
             return 0;
         }},
        {"factory-json", "GeneralFactory + Json 配置", Label::UNIT,
         [] {
             FactoryJsonTest();
             return 0;
         }},
        {"sync-agency", "SyncAgency 跨模块同步", Label::UNIT,
         [] {
             TestSyncAgency();
             return 0;
         }},
        {"thread-pool", "ThreadPool", Label::UNIT,
         [] {
             ThreadPoolTest();
             return 0;
         }},
        {"instance-counter", "InstanceCounter", Label::UNIT,
         [] {
             InstanceCounterTest();
             return 0;
         }},
        {"type2instmap", "Type2InstMap", Label::UNIT,
         [] {
             Type2InstMapTest();
             return 0;
         }},
        {"magic-enum", "MagicEnum 枚举转换", Label::UNIT,
         [] {
             MagicEnumTest::TestMagicEnum();
             return 0;
         }},

#if VAS_HAS_STDEXEC
        {"vas-concurrency", "stdexec 并发管线", Label::UNIT,
         [] {
             VasConcurrencyTest();
             return 0;
         }},
#endif

        // ---------- OpenCV / FFmpeg / exiv2 ----------
        {"exif", "Exiv2 读取 EXIF", Label::UNIT,
         [] {
             CameraUtils::Exif2Test();
             return 0;
         }},

        // ---------- 依赖媒体素材 ----------
        {"ffmpeg", "FFmpeg 基础解码", Label::MEDIA,
         [] {
             TestFFMpeg();
             return 0;
         }},
        {"optical-flow-upsample", "光流 30→60 帧插值", Label::MEDIA, [] { return Upsample30To60(); }},
        {"hdr-merge", "HDR 融合", Label::MEDIA,
         [] {
             RunHdrMergeTest();
             return 0;
         }},
        {"hdr-merge-multilevel", "HDR 融合（多档位）", Label::MEDIA,
         [] {
             RunHdrMergeMultiLevelTest();
             return 0;
         }},
        {"hdr-merge-physical", "HDR 物理融合", Label::MEDIA,
         [] {
             RunHdrMergePhysicalTest();
             return 0;
         }},
        {"hdr-merge-ghost", "HDR 鬼影抑制", Label::MEDIA,
         [] {
             RunHdrMergeGhostSuppressionTest();
             return 0;
         }},
        {"livephoto", "LivePhoto 平滑播放", Label::MEDIA,
         [] {
             RunLivePhotoSmoothPlayTest();
             return 0;
         }},
        {"livephoto-reg", "LivePhoto 图像配准", Label::MEDIA,
         [] {
             RegTest();
             return 0;
         }},

        // ---------- 需要图形界面 ----------
        {"opencv-basic", "OpenCV 基础读写与显示", Label::GUI,
         [] {
             OpenCVTest();
             return 0;
         }},
        {"optical-flow", "光流可视化（交互窗口）", Label::GUI, [] { return OpticalFlowTest(); }},
        {"harris", "Harris 角点检测可视化", Label::GUI,
         [] {
             HarrisTest();
             return 0;
         }},
    };
    return commands;
}

} // namespace Cli

#endif // TESTS_CLI_COMMANDTABLE_H

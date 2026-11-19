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
        {.name = "harris",
         .description = "Harris 角点检测可视化",
         .label = Label::GUI,
         .run =
             [] {
                 HarrisTest();
                 return 0;
             }},
    };
    return commands;
}

} // namespace Cli

#endif // TESTS_CLI_COMMANDTABLE_H

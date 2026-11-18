#ifndef CLI_COMMANDTABLE_H
#define CLI_COMMANDTABLE_H

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

namespace cli {

/// 命令分类：决定是否依赖外部资源
enum class Label {
    Unit,  ///< 纯内存运算，无外部依赖
    Media, ///< 需要 data/ 目录下的媒体素材
    Gui    ///< 需要图形界面（imshow / waitKey）
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
    case Label::Unit:
        return "unit";
    case Label::Media:
        return "media";
    case Label::Gui:
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
        {"deque-blocking", "BlockingDeque 阻塞模式", Label::Unit,
         [] {
             TestInBlockingMode();
             return 0;
         }},
        {"deque-nonblocking", "BlockingDeque 非阻塞模式", Label::Unit,
         [] {
             TestInNonBlockingMode();
             return 0;
         }},
        {"find-then-or-else", "Find/AndThen/OrElse", Label::Unit,
         [] {
             TestFindAndThenOrElse();
             return 0;
         }},
        {"overload-ref", "Overload 引用重载", Label::Unit,
         [] {
             TestOverloadRef();
             return 0;
         }},
        {"timeout-monitor", "TimeoutMonitor", Label::Unit,
         [] {
             TimeoutMonitorTest();
             return 0;
         }},
        {"misc", "Misc：Enum2Str / Scoped / InstanceCounter", Label::Unit,
         [] {
             MiscTest();
             return 0;
         }},
        {"circular-queue", "CircularQueue", Label::Unit,
         [] {
             TestCircularQueue();
             return 0;
         }},
        {"overwrite-function", "OverwriteFunction", Label::Unit,
         [] {
             TestOverwriteFunction();
             return 0;
         }},
        {"msg-queue", "MsgQueue 任务队列", Label::Unit,
         [] {
             MsgQueueTest();
             return 0;
         }},
        {"event", "Event/EventDispatcher", Label::Unit,
         [] {
             TestEvent();
             return 0;
         }},
        {"factory-json", "GeneralFactory + Json 配置", Label::Unit,
         [] {
             FactoryJsonTest();
             return 0;
         }},
        {"sync-agency", "SyncAgency 跨模块同步", Label::Unit,
         [] {
             TestSyncAgency();
             return 0;
         }},
        {"thread-pool", "ThreadPool", Label::Unit,
         [] {
             ThreadPoolTest();
             return 0;
         }},
        {"instance-counter", "InstanceCounter", Label::Unit,
         [] {
             InstanceCounterTest();
             return 0;
         }},
        {"type2instmap", "Type2InstMap", Label::Unit,
         [] {
             Type2InstMapTest();
             return 0;
         }},
        {"magic-enum", "MagicEnum 枚举转换", Label::Unit,
         [] {
             MagicEnumTest::TestMagicEnum();
             return 0;
         }},

#if VAS_HAS_STDEXEC
        {"vas-concurrency", "stdexec 并发管线", Label::Unit,
         [] {
             VasConcurrencyTest();
             return 0;
         }},
#endif

        // ---------- OpenCV / FFmpeg / exiv2 ----------
        {"exif", "Exiv2 读取 EXIF", Label::Unit,
         [] {
             CameraUtils::Exif2Test();
             return 0;
         }},

        // ---------- 依赖媒体素材 ----------
        {"ffmpeg", "FFmpeg 基础解码", Label::Media,
         [] {
             TestFFMpeg();
             return 0;
         }},
        {"optical-flow-upsample", "光流 30→60 帧插值", Label::Media, [] { return Upsample30To60(); }},
        {"hdr-merge", "HDR 融合", Label::Media,
         [] {
             RunHdrMergeTest();
             return 0;
         }},
        {"hdr-merge-multilevel", "HDR 融合（多档位）", Label::Media,
         [] {
             RunHdrMergeMultiLevelTest();
             return 0;
         }},
        {"hdr-merge-physical", "HDR 物理融合", Label::Media,
         [] {
             RunHdrMergePhysicalTest();
             return 0;
         }},
        {"hdr-merge-ghost", "HDR 鬼影抑制", Label::Media,
         [] {
             RunHdrMergeGhostSuppressionTest();
             return 0;
         }},
        {"livephoto", "LivePhoto 平滑播放", Label::Media,
         [] {
             RunLivePhotoSmoothPlayTest();
             return 0;
         }},
        {"livephoto-reg", "LivePhoto 图像配准", Label::Media,
         [] {
             RegTest();
             return 0;
         }},

        // ---------- 需要图形界面 ----------
        {"opencv-basic", "OpenCV 基础读写与显示", Label::Gui,
         [] {
             OpenCVTest();
             return 0;
         }},
        {"optical-flow", "光流可视化（交互窗口）", Label::Gui, [] { return OpticalFlowTest(); }},
        {"harris", "Harris 角点检测可视化", Label::Gui,
         [] {
             HarrisTest();
             return 0;
         }},
    };
    return commands;
}

} // namespace cli

#endif // CLI_COMMANDTABLE_H

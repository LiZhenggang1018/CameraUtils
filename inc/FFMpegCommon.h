#ifndef FFMPEGCOMMON_H
#define FFMPEGCOMMON_H	

// 必须放在 extern "C" 之前，确保 <windows.h> 第一次以 C++ linkage 被包含
#ifndef NOMINMAX
#define NOMINMAX            // 避免 min/max 宏与 OpenCV/STL 冲突
#endif
#include <windows.h>

// FFmpeg 是 C 库，必须用 extern "C" 包裹，否则 C++ 下会链接失败
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#endif // !FFMPEGCOMMON_H

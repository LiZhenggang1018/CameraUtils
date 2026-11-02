#ifndef FFMPEGCOMMON_H
#define FFMPEGCOMMON_H	

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

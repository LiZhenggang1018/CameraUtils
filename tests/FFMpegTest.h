#ifndef FFMPEGTEST_H
#define FFMPEGTEST_H

#include <iostream>
#include <string>
#include "FFMpegCommon.h" // 由 src/CMakeLists.txt 暴露的 tests/inc 路径找到

inline void TestFFMpeg()
{
    // 1. 打印版本
    std::cout << "FFmpeg version: " << av_version_info() << std::endl;
    std::cout << "avcodec  version: " << avcodec_version() << std::endl;
    std::cout << "avformat version: " << avformat_version() << std::endl;

    // 2. 尝试打开一个媒体文件（视频或图片都可以）
    // 用你已有的 data 目录下的文件，或者换成任意一个 mp4/avi
    const std::string file =
        std::string(PROJECT_ROOT_DIR) + "/data/lhc.jpg"; // 如果没有视频，jpg 也能被 avformat 识别

    AVFormatContext* fmtCtx = nullptr;
    int ret = avformat_open_input(&fmtCtx, file.c_str(), nullptr, nullptr);
    if (ret < 0)
    {
        char errBuf[AV_ERROR_MAX_STRING_SIZE] = {};
        av_strerror(ret, errBuf, sizeof(errBuf));
        std::cerr << "avformat_open_input failed: " << errBuf << std::endl;
        return;
    }

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0)
    {
        std::cerr << "avformat_find_stream_info failed" << std::endl;
        avformat_close_input(&fmtCtx);
        return;
    }

    std::cout << "File: " << file << std::endl;
    std::cout << "Format: " << fmtCtx->iformat->name << std::endl;
    std::cout << "Duration: " << fmtCtx->duration / AV_TIME_BASE << " s" << std::endl;
    std::cout << "Streams: " << fmtCtx->nb_streams << std::endl;

    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i)
    {
        AVStream* st = fmtCtx->streams[i];
        const char* type = av_get_media_type_string(st->codecpar->codec_type);
        std::cout << "  stream " << i << ": " << (type ? type : "unknown")
                  << ", codec_id=" << st->codecpar->codec_id << std::endl;
    }

    avformat_close_input(&fmtCtx);
}

#endif // !FFMPEGTEST_H

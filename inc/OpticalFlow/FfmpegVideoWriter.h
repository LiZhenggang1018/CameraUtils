#ifndef FFMPEG_VIDEO_WRITER_H
#define FFMPEG_VIDEO_WRITER_H

#include "FFMpegCommon.h"
#include "OpenCVCommon.h"

#include <string>

// 基于 FFmpeg C API 的 MP4 写入器（H.264 / libx264）
// 用法：
//   FfmpegVideoWriter writer;
//   writer.Open("out.mp4", w, h, 60, 8'000'000);
//   writer.WriteFrame(bgrMat);   // 每帧调用一次
//   writer.Close();              // 必须调用，否则 MP4 没有 moov atom
class FfmpegVideoWriter {
public:
    FfmpegVideoWriter() = default;
    ~FfmpegVideoWriter() { Close(); }

    FfmpegVideoWriter(const FfmpegVideoWriter&) = delete;
    FfmpegVideoWriter& operator=(const FfmpegVideoWriter&) = delete;

    bool Open(const std::string& path, int width, int height,
              int fps, int bitrate = 8000000);

    bool WriteFrame(const cv::Mat& bgr);

    void Close();

    [[nodiscard]] bool IsOpened() const { return m_opened; }
    [[nodiscard]] const char* EncoderName() const { return m_encoderName; }

private:
    AVFormatContext* m_fmtCtx   = nullptr;
    AVCodecContext*  m_codecCtx = nullptr;
    AVStream*        m_stream   = nullptr;
    SwsContext*      m_swsCtx   = nullptr;
    AVFrame*         m_frame    = nullptr;
    AVPacket*        m_packet   = nullptr;

    int64_t m_pts = 0;
    bool    m_opened = false;
    const char* m_encoderName = "";
};

#endif // FFMPEG_VIDEO_WRITER_H
#ifndef FFMPEG_VIDEO_WRITER_H
#define FFMPEG_VIDEO_WRITER_H

#include "FFMpegCommon.h"
#include "OpenCVCommon.h"

#include <string>

// 基于 FFmpeg C API 的 MP4 写入器（H.264 / libx264）
// 用法：
//   FfmpegVideoWriter writer;
//   writer.Open("out.mp4", w, h, 60, 16000000);
//   writer.WriteFrame(bgrMat);
//   writer.Close();          // 必须调用，否则 MP4 没有 moov atom
class FfmpegVideoWriter {
public:
    // 编码器可选参数
    struct EncoderParams {
        std::string preset = "veryfast";   // libx264 preset：ultrafast/veryfast/fast/medium/slow...
        int         crf    = -1;           // >= 0 时用 CRF 模式，忽略 bitrate；-1 时用 ABR
    };

    FfmpegVideoWriter() = default;
    ~FfmpegVideoWriter() { Close(); }

    FfmpegVideoWriter(const FfmpegVideoWriter&) = delete;
    FfmpegVideoWriter& operator=(const FfmpegVideoWriter&) = delete;

    // bitrate 只在 crf < 0 时生效
    bool Open(const std::string& path, int width, int height,
              int fps, int bitrate = 8000000,
              const EncoderParams& enc = EncoderParams{});

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
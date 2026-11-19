#ifndef INC_OPTICALFLOW_FFMPEGVIDEOWRITER_H
#define INC_OPTICALFLOW_FFMPEGVIDEOWRITER_H

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
        std::string preset = "veryfast"; // libx264 preset：ultrafast/veryfast/fast/medium/slow...
        int crf = -1;                    // >= 0 时用 CRF 模式，忽略 bitrate；-1 时用 ABR
    };

    FfmpegVideoWriter() = default;
    ~FfmpegVideoWriter() { Close(); }

    FfmpegVideoWriter(const FfmpegVideoWriter&) = delete;
    FfmpegVideoWriter& operator=(const FfmpegVideoWriter&) = delete;

    // bitrate 只在 crf < 0 时生效
    bool Open(const std::string& path, int width, int height, int fps, int bitrate = 8000000);

    bool Open(const std::string& path, int width, int height, int fps, int bitrate, const EncoderParams& enc);

    bool WriteFrame(const cv::Mat& bgr);

    void Close();

    [[nodiscard]] bool IsOpened() const { return opened_; }
    [[nodiscard]] const char* EncoderName() const { return encoderName_; }

private:
    AVFormatContext* fmtCtx_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    AVStream* stream_ = nullptr;
    SwsContext* swsCtx_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVPacket* packet_ = nullptr;

    int64_t pts_ = 0;
    bool opened_ = false;
    const char* encoderName_ = "";
};

#endif // INC_OPTICALFLOW_FFMPEGVIDEOWRITER_H
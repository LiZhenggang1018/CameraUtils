#include "FfmpegVideoWriter.h"

#include <cstdio>
#include <cstring>

namespace {

std::string AvErr(int err) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(err, buf, sizeof(buf));
    return std::string(buf);
}

} // namespace

bool FfmpegVideoWriter::Open(const std::string& path, int width, int height,
                             int fps, int bitrate) {
    Close();

    if (width <= 0 || height <= 0 || fps <= 0) {
        std::fprintf(stderr, "[FfmpegVideoWriter] invalid params\n");
        return false;
    }

    // 1. 输出上下文（根据扩展名自动选 mp4 封装）
    int ret = avformat_alloc_output_context2(&m_fmtCtx, nullptr, nullptr, path.c_str());
    if (ret < 0 || m_fmtCtx == nullptr) {
        std::fprintf(stderr, "[FfmpegVideoWriter] alloc_output_context2: %s\n",
                     AvErr(ret).c_str());
        return false;
    }

    // 2. 找 H.264 编码器：优先 libx264（BtbN gpl-shared 自带），否则内置 H264
    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (codec == nullptr) {
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    if (codec == nullptr) {
        std::fprintf(stderr, "[FfmpegVideoWriter] no H.264 encoder found\n");
        Close();
        return false;
    }
    m_encoderName = codec->name;

    // 3. 编码器上下文
    m_codecCtx = avcodec_alloc_context3(codec);
    if (m_codecCtx == nullptr) {
        std::fprintf(stderr, "[FfmpegVideoWriter] alloc_codec_context3 failed\n");
        Close();
        return false;
    }

    m_codecCtx->width      = width;
    m_codecCtx->height     = height;
    m_codecCtx->pix_fmt    = AV_PIX_FMT_YUV420P;
    m_codecCtx->time_base  = AVRational{1, fps};
    m_codecCtx->framerate  = AVRational{fps, 1};
    m_codecCtx->gop_size   = 10;
    m_codecCtx->max_b_frames = 1;
    m_codecCtx->bit_rate   = bitrate;

    // MP4 需要全局头
    if (m_fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(m_codecCtx, codec, nullptr);
    if (ret < 0) {
        std::fprintf(stderr, "[FfmpegVideoWriter] avcodec_open2: %s\n",
                     AvErr(ret).c_str());
        Close();
        return false;
    }

    // 4. 流
    m_stream = avformat_new_stream(m_fmtCtx, nullptr);
    if (m_stream == nullptr) {
        std::fprintf(stderr, "[FfmpegVideoWriter] avformat_new_stream failed\n");
        Close();
        return false;
    }
    ret = avcodec_parameters_from_context(m_stream->codecpar, m_codecCtx);
    if (ret < 0) {
        std::fprintf(stderr, "[FfmpegVideoWriter] parameters_from_context: %s\n",
                     AvErr(ret).c_str());
        Close();
        return false;
    }
    m_stream->time_base = m_codecCtx->time_base;

    // 5. 打开输出文件
    ret = avio_open(&m_fmtCtx->pb, path.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
        std::fprintf(stderr, "[FfmpegVideoWriter] avio_open: %s\n",
                     AvErr(ret).c_str());
        Close();
        return false;
    }

    // 6. 写文件头（moov 通常在 trailer 写，faststart 需要 seek）
    ret = avformat_write_header(m_fmtCtx, nullptr);
    if (ret < 0) {
        std::fprintf(stderr, "[FfmpegVideoWriter] write_header: %s\n",
                     AvErr(ret).c_str());
        Close();
        return false;
    }

    // 7. 像素格式转换
    m_swsCtx = sws_getContext(
        width, height, AV_PIX_FMT_BGR24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (m_swsCtx == nullptr) {
        std::fprintf(stderr, "[FfmpegVideoWriter] sws_getContext failed\n");
        Close();
        return false;
    }

    // 8. 帧与包
    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();
    if (m_frame == nullptr || m_packet == nullptr) {
        std::fprintf(stderr, "[FfmpegVideoWriter] alloc frame/packet failed\n");
        Close();
        return false;
    }
    m_frame->format = m_codecCtx->pix_fmt;
    m_frame->width  = width;
    m_frame->height = height;
    ret = av_frame_get_buffer(m_frame, 0);
    if (ret < 0) {
        std::fprintf(stderr, "[FfmpegVideoWriter] av_frame_get_buffer: %s\n",
                     AvErr(ret).c_str());
        Close();
        return false;
    }

    m_pts = 0;
    m_opened = true;
    std::printf("[FfmpegVideoWriter] opened: %s, encoder=%s, %dx%d @ %dfps\n",
                path.c_str(), m_encoderName, width, height, fps);
    return true;
}

bool FfmpegVideoWriter::WriteFrame(const cv::Mat& bgr) {
    if (!m_opened) {
        std::fprintf(stderr, "[WF] not opened\n");
        return false;
    }
    if (bgr.empty() || bgr.type() != CV_8UC3) {
        std::fprintf(stderr, "[WF] expect CV_8UC3, got type=%d empty=%d\n",
                     bgr.type(), static_cast<int>(bgr.empty()));
        return false;
    }

    std::printf("[WF] enter pts=%lld, frame %dx%d\n",
                static_cast<long long>(m_pts), bgr.cols, bgr.rows);

    // ---- 检查 m_frame 的 3 个 plane ----
    if (m_frame->data[0] == nullptr || m_frame->data[1] == nullptr ||
        m_frame->data[2] == nullptr) {
        std::fprintf(stderr, "[WF] m_frame planes invalid: %p %p %p\n",
                     static_cast<void*>(m_frame->data[0]),
                     static_cast<void*>(m_frame->data[1]),
                     static_cast<void*>(m_frame->data[2]));
        return false;
    }
    std::printf("[WF] linesize: %d %d %d\n",
                m_frame->linesize[0], m_frame->linesize[1], m_frame->linesize[2]);

    // ---- BGR -> YUV420P ----
    const uint8_t* srcData[1] = { bgr.data };
    const int      srcLine[1] = { static_cast<int>(bgr.step) };
    std::printf("[WF] sws_scale begin, src step=%d\n", srcLine[0]);
    const int scaled = sws_scale(m_swsCtx,
                                 srcData, srcLine, 0, bgr.rows,
                                 m_frame->data, m_frame->linesize);
    std::printf("[WF] sws_scale end, returned=%d\n", scaled);

    m_frame->pts = m_pts++;

    // ---- send frame ----
    std::printf("[WF] send_frame pts=%lld\n", static_cast<long long>(m_frame->pts));
    int ret = avcodec_send_frame(m_codecCtx, m_frame);
    std::printf("[WF] send_frame ret=%d (%s)\n", ret,
                ret < 0 ? AvErr(ret).c_str() : "OK");

    if (ret == AVERROR(EAGAIN)) {
        std::fprintf(stderr, "[WF] encoder full (EAGAIN), drain first\n");
        // 先 drain
        while (true) {
            int rr = avcodec_receive_packet(m_codecCtx, m_packet);
            if (rr == AVERROR(EAGAIN) || rr == AVERROR_EOF) break;
            if (rr < 0) {
                std::fprintf(stderr, "[WF] drain recv: %s\n", AvErr(rr).c_str());
                return false;
            }
            av_packet_rescale_ts(m_packet, m_codecCtx->time_base, m_stream->time_base);
            m_packet->stream_index = m_stream->index;
            av_interleaved_write_frame(m_fmtCtx, m_packet);
            av_packet_unref(m_packet);
        }
        // 重试一次
        ret = avcodec_send_frame(m_codecCtx, m_frame);
        std::printf("[WF] send_frame retry ret=%d\n", ret);
        if (ret < 0) {
            std::fprintf(stderr, "[WF] send_frame retry: %s\n", AvErr(ret).c_str());
            return false;
        }
    } else if (ret < 0) {
        std::fprintf(stderr, "[WF] send_frame: %s\n", AvErr(ret).c_str());
        return false;
    }

    // ---- receive packets ----
    while (true) {
        ret = avcodec_receive_packet(m_codecCtx, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) {
            std::fprintf(stderr, "[WF] recv_packet: %s\n", AvErr(ret).c_str());
            return false;
        }
        std::printf("[WF] got packet size=%d\n", m_packet->size);

        av_packet_rescale_ts(m_packet, m_codecCtx->time_base, m_stream->time_base);
        m_packet->stream_index = m_stream->index;

        int wret = av_interleaved_write_frame(m_fmtCtx, m_packet);
        if (wret < 0) {
            std::fprintf(stderr, "[WF] write_frame: %s\n", AvErr(wret).c_str());
            return false;
        }
        std::printf("[WF] packet written\n");
        av_packet_unref(m_packet);
    }
    std::printf("[WF] done pts=%lld\n", static_cast<long long>(m_pts));
    return true;
}

void FfmpegVideoWriter::Close() {
    if (m_codecCtx != nullptr && m_opened) {
        // flush 编码器
        avcodec_send_frame(m_codecCtx, nullptr);
        while (true) {
            const int ret = avcodec_receive_packet(m_codecCtx, m_packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                break;
            }
            av_packet_rescale_ts(m_packet, m_codecCtx->time_base, m_stream->time_base);
            m_packet->stream_index = m_stream->index;
            av_interleaved_write_frame(m_fmtCtx, m_packet);
        }
    }

    if (m_fmtCtx != nullptr && (m_fmtCtx->pb != nullptr)) {
        // 关键：写 trailer（moov atom 在这里）
        const int ret = av_write_trailer(m_fmtCtx);
        if (ret < 0) {
            std::fprintf(stderr, "[FfmpegVideoWriter] write_trailer: %s\n",
                         AvErr(ret).c_str());
        }
        avio_closep(&m_fmtCtx->pb);
    }

    if (m_swsCtx)   { sws_freeContext(m_swsCtx);   m_swsCtx = nullptr; }
    if (m_frame)    { av_frame_free(&m_frame);     }
    if (m_packet)   { av_packet_free(&m_packet);   }
    if (m_codecCtx) { avcodec_free_context(&m_codecCtx); }
    if (m_fmtCtx)   { avformat_free_context(m_fmtCtx);   m_fmtCtx = nullptr; }

    m_stream = nullptr;
    m_opened = false;
}
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

    // 显式指定色彩空间，让 mp4 metadata 标记为 BT.709
    m_codecCtx->color_primaries = AVCOL_PRI_BT709;   // 原色
    m_codecCtx->color_trc       = AVCOL_TRC_BT709;   // 传输特性
    m_codecCtx->colorspace      = AVCOL_SPC_BT709;   // 矩阵系数
    m_codecCtx->color_range     = AVCOL_RANGE_MPEG;  // limited range (16-235)

    // MP4 需要全局头
    if (m_fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        m_codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // libx264 快速预设（其他编码器会忽略未知 key）
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "preset", "medium", 0);   // veryfast → medium，画质提升明显
    av_dict_set(&opts, "crf", "20", 0);          // CRF 18~23 是"视觉无损"范围

    ret = avcodec_open2(m_codecCtx, codec, &opts);
    av_dict_free(&opts); // 必须释放，否则内存泄漏
    if (ret < 0)
    {
        std::fprintf(stderr, "[FfmpegVideoWriter] avcodec_open2: %s\n", AvErr(ret).c_str());
        Close();
        return false;
    }
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
    // 由于我的输入源文件是 BT.709，故在这里显式指定 BT.709 + limited range
    m_swsCtx = sws_getContext(
        width, height, AV_PIX_FMT_BGR24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    if (m_swsCtx != nullptr) {
        // 显式指定 BT.709 矩阵 + limited range
        // 源 BGR24: full range (1)
        // 目标 YUV420P: limited range (0)
        const int srcRange = 1;   // full
        const int dstRange = 0;   // limited (16-235)
        const int* srcCoeff = sws_getCoefficients(SWS_CS_ITU709);
        const int* dstCoeff = sws_getCoefficients(SWS_CS_ITU709);
        int brightness = 0;
        int contrast   = 1 << 16;
        int saturation = 1 << 16;
        sws_setColorspaceDetails(m_swsCtx,
            srcCoeff, srcRange,
            dstCoeff, dstRange,
            brightness, contrast, saturation);
    }
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
        std::fprintf(stderr, "[FfmpegVideoWriter] not opened\n");
        return false;
    }
    if (bgr.empty() || bgr.type() != CV_8UC3) {
        std::fprintf(stderr, "[FfmpegVideoWriter] expect CV_8UC3, got type=%d empty=%d\n",
                     bgr.type(), static_cast<int>(bgr.empty()));
        return false;
    }

    // ---- BGR -> YUV420P ----
    const uint8_t* srcData[1] = { bgr.data };
    const int      srcLine[1] = { static_cast<int>(bgr.step) };
    sws_scale(m_swsCtx, srcData, srcLine, 0, bgr.rows,
              m_frame->data, m_frame->linesize);

    m_frame->pts = m_pts++;

    // ---- send frame ----
    int ret = avcodec_send_frame(m_codecCtx, m_frame);
    if (ret == AVERROR(EAGAIN)) {
        // 编码器缓冲区满，先取完再重试
        while (true) {
            int rr = avcodec_receive_packet(m_codecCtx, m_packet);
            if (rr == AVERROR(EAGAIN) || rr == AVERROR_EOF) break;
            if (rr < 0) {
                std::fprintf(stderr, "[FfmpegVideoWriter] drain recv: %s\n",
                             AvErr(rr).c_str());
                return false;
            }
            av_packet_rescale_ts(m_packet, m_codecCtx->time_base, m_stream->time_base);
            m_packet->stream_index = m_stream->index;
            av_interleaved_write_frame(m_fmtCtx, m_packet);
            av_packet_unref(m_packet);
        }
        ret = avcodec_send_frame(m_codecCtx, m_frame);
    }

    if (ret < 0) {
        std::fprintf(stderr, "[FfmpegVideoWriter] send_frame: %s\n",
                     AvErr(ret).c_str());
        return false;
    }

    // ---- receive packets ----
    while (true) {
        ret = avcodec_receive_packet(m_codecCtx, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) {
            std::fprintf(stderr, "[FfmpegVideoWriter] recv_packet: %s\n",
                         AvErr(ret).c_str());
            return false;
        }
        av_packet_rescale_ts(m_packet, m_codecCtx->time_base, m_stream->time_base);
        m_packet->stream_index = m_stream->index;

        int wret = av_interleaved_write_frame(m_fmtCtx, m_packet);
        if (wret < 0) {
            std::fprintf(stderr, "[FfmpegVideoWriter] write_frame: %s\n",
                         AvErr(wret).c_str());
            return false;
        }
        av_packet_unref(m_packet);
    }
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
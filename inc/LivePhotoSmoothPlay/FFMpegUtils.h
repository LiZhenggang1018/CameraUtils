/*
 * @Description: 用于打印视频文件的I P B 帧数
 */

#ifndef INC_LIVEPHOTOSMOOTHPLAY_FFMPEGUTILS_H
#define INC_LIVEPHOTOSMOOTHPLAY_FFMPEGUTILS_H

#include "DumpUtils.h"
#include "FFmpegDefine.h"

// 返回 input_file 视频文件中的 I/P/B 帧数（已 flush 解码器，统计完整）
inline std::tuple<int, int, int> CountFrames(const std::string& inputFile)
{
    const std::tuple<int, int, int> errResult{-1, -1, -1};

    AVFormatContext* rawFmtCtx{};
    if (avformat_open_input(&rawFmtCtx, inputFile.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Could not open input file: " << inputFile << std::endl;
        return errResult;
    }
    AVFormatContextPtr formatCtx(rawFmtCtx); // 统一用 FFmpegDefine.h 的 RAII 类型

    if (avformat_find_stream_info(formatCtx.get(), nullptr) < 0) {
        std::cerr << "Could not find stream information." << std::endl;
        return errResult;
    }

    // 查找视频流
    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatCtx->nb_streams; ++i) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = static_cast<int>(i);
            break;
        }
    }
    if (videoStreamIndex == -1) {
        std::cerr << "Could not find a video stream." << std::endl;
        return errResult;
    }

    AVCodecParameters* codecpar = formatCtx->streams[videoStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        std::cerr << "Unsupported codec!" << std::endl;
        return errResult;
    }

    AVCodecContext* rawCodecCtx = avcodec_alloc_context3(codec);
    if (!rawCodecCtx) {
        std::cerr << "Could not allocate codec context." << std::endl;
        return errResult;
    }
    AVCodecContextPtr codecCtx(rawCodecCtx);

    if (avcodec_parameters_to_context(codecCtx.get(), codecpar) < 0) {
        std::cerr << "Could not copy codec parameters to context." << std::endl;
        return errResult;
    }
    if (avcodec_open2(codecCtx.get(), codec, nullptr) < 0) {
        std::cerr << "Could not open codec." << std::endl;
        return errResult;
    }

    AVPacket packet{};
    AVFrame* rawFrame = av_frame_alloc();
    if (!rawFrame) {
        std::cerr << "Could not allocate frame." << std::endl;
        return errResult;
    }
    AVFramePtr frame(rawFrame); // 用 RAII，避免异常路径漏释放

    // 统计计数器（含"其他类型"，保证 total 准确）
    int iFrameCnt = 0;
    int pFrameCnt = 0;
    int bFrameCnt = 0;
    int otherFrameCnt = 0;

    // 统计分类逻辑抽成 lambda，主循环和 flush 循环共用
    auto classifyAndCount = [&](const AVFrame* f) {
        switch (f->pict_type) {
        case AV_PICTURE_TYPE_I:
            ++iFrameCnt;
            break;
        case AV_PICTURE_TYPE_P:
            ++pFrameCnt;
            break;
        case AV_PICTURE_TYPE_B:
            ++bFrameCnt;
            break;
        default:
            ++otherFrameCnt;
            break; // NONE / S / SI / SP
        }
    };

    // ============ 主循环：读包 → 解码 → 统计 ============
    while (av_read_frame(formatCtx.get(), &packet) >= 0) {
        if (packet.stream_index == videoStreamIndex) {
            int ret = avcodec_send_packet(codecCtx.get(), &packet);
            if (ret < 0) {
                std::cerr << "Error sending packet for decoding: " << ret << std::endl;
                av_packet_unref(&packet);
                break;
            }

            while (true) {
                ret = avcodec_receive_frame(codecCtx.get(), frame.get());
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                if (ret < 0) {
                    std::cerr << "Error during decoding: " << ret << std::endl;
                    break;
                }
                classifyAndCount(frame.get());
            }
        }
        av_packet_unref(&packet);
    }

    // ============ ★ 关键修复：flush decoder ============
    // 发送 nullptr 触发排空，然后循环取出 reorder buffer 里缓存的最后 1~3 帧
    int flushRet = avcodec_send_packet(codecCtx.get(), nullptr);
    if (flushRet < 0 && flushRet != AVERROR_EOF) {
        std::cerr << "Error flushing decoder: " << flushRet << std::endl;
    }

    while (true) {
        int ret = avcodec_receive_frame(codecCtx.get(), frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            std::cerr << "Error receiving frame during flush: " << ret << std::endl;
            break;
        }
        classifyAndCount(frame.get());
    }

    // ============ 输出统计 ============
    const auto totalFrame = iFrameCnt + pFrameCnt + bFrameCnt + otherFrameCnt;
    PV(inputFile, "I:", iFrameCnt, "P:", pFrameCnt, "B:", bFrameCnt, "Other:", otherFrameCnt, "Total:", totalFrame);

    return std::make_tuple(iFrameCnt, pFrameCnt, bFrameCnt);
}

#endif // INC_LIVEPHOTOSMOOTHPLAY_FFMPEGUTILS_H
/*
 * @Description: 用于打印视频文件的I P B 帧数
 */

#ifndef FFMPEGUTILS_H_
#define FFMPEGUTILS_H_

#include "FFmpegDefine.h"
#include "DumpUtils.h"

// 返回input_file视频文件中的I P B 帧数
inline std::tuple<int, int, int> CountFrames(const std::string &input_file)
{
    const std::tuple<int, int, int> errResult{-1, -1, -1};

    AVFormatContext *raw_fmt_ctx{};
    if (avformat_open_input(&raw_fmt_ctx, input_file.c_str(), nullptr, nullptr) != 0)
    {
        std::cerr << "Could not open input file." << std::endl;
        return errResult;
    }
    std::unique_ptr<AVFormatContext, void (*)(AVFormatContext*)> format_ctx(
        raw_fmt_ctx, [](AVFormatContext* ctx) { avformat_close_input(&ctx); });

    if (avformat_find_stream_info(format_ctx.get(), nullptr) < 0)
    {
        std::cerr << "Could not find stream information." << std::endl;
        return errResult;
    }

    // 查找视频流
    int video_stream_index{-1};
    for (unsigned int i = 0; i < format_ctx->nb_streams; ++i)
    {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index = static_cast<int>(i);
            break;
        }
    }

    if (video_stream_index == -1)
    {
        std::cerr << "Could not find a video stream." << std::endl;
        return errResult;
    }

    AVCodecParameters *codecpar = format_ctx->streams[video_stream_index]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec)
    {
        std::cerr << "Unsupported codec!" << std::endl;
        return errResult;
    }

    AVCodecContext *raw_codec_ctx{};
    raw_codec_ctx = avcodec_alloc_context3(codec);
    if (!raw_codec_ctx)
    {
        std::cerr << "Could not allocate codec context." << std::endl;
        return errResult;
    }
    // 修正 unique_ptr 构造函数参数类型，需指定类型别名
    AVCodecContextPtr codec_ctx(raw_codec_ctx);

    if (avcodec_parameters_to_context(codec_ctx.get(), codecpar) < 0)
    {
        std::cerr << "Could not copy codec parameters to context." << std::endl;
        return errResult;
    }

    if (avcodec_open2(codec_ctx.get(), codec, nullptr) < 0)
    {
        std::cerr << "Could not open codec." << std::endl;
        return errResult;
    }

    AVPacket packet{};
    AVFrame *frame{};
    frame = av_frame_alloc();
    if (!frame)
    {
        std::cerr << "Could not allocate frame." << std::endl;
        return errResult;
    }

    int ret{};
    int iFrameCnt{}, pFrameCnt{}, bFrameCnt{};
    while (av_read_frame(format_ctx.get(), &packet) >= 0)
    {
        if (packet.stream_index == video_stream_index)
        {
            ret = avcodec_send_packet(codec_ctx.get(), &packet);
            if (ret < 0)
            {
                std::cerr << "Error sending packet for decoding." << std::endl;
                av_packet_unref(&packet);
                break;
            }

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(codec_ctx.get(), frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    std::cerr << "Error during decoding." << std::endl;
                    break;
                }

                // 统计帧类型
                if (frame->pict_type == AV_PICTURE_TYPE_I)
                {
                    ++iFrameCnt;
                }
                else if (frame->pict_type == AV_PICTURE_TYPE_P)
                {
                    ++pFrameCnt;
                }
                else if (frame->pict_type == AV_PICTURE_TYPE_B)
                {
                    ++bFrameCnt;
                }
            }
        }
        av_packet_unref(&packet);
    }

    av_frame_free(&frame);
    auto totalFrame = iFrameCnt + pFrameCnt + bFrameCnt;
    PV(input_file, "has frame I P B Total:", iFrameCnt, pFrameCnt, bFrameCnt, totalFrame);
    return std::make_tuple(iFrameCnt, pFrameCnt, bFrameCnt);
}

#endif // FFMPEGUTILS_H_
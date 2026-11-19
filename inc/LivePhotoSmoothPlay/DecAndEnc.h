/*
 * @Description: 用于执行FFmpeg编解码操作。且编码时根据使用场景决定在视频结尾插入封面帧 或者 FOV过渡帧
 */

#ifndef INC_LIVEPHOTOSMOOTHPLAY_DECANDENC_H
#define INC_LIVEPHOTOSMOOTHPLAY_DECANDENC_H

// 用于执行FFmpeg编解码操作。且编码时根据使用场景决定在视频结尾插入封面帧 或者 FOV过渡帧
#include <filesystem>
#include <iostream>
#include <memory>

#include "FFMpegUtils.h"
#include "FFmpegDefine.h"

namespace fs = std::filesystem;

struct AVStreamTempInfo {
    AVRational timeBase{};
    int height{};
    int width{};
    AVRational sampleAspectRatio{};
    int64_t bitRate{};
    AVPixelFormat pixFmt{};
    AVRational framerate{};
};

// 获取视频流的临时信息辅助函数，如time_base、分辨率、像素格式等
inline AVStreamTempInfo GetAVStreamTempInfo(const std::string& filename)
{
    AVFormatContext* rawFmtCtx{};
    AVStreamTempInfo info{};

    // 打开输入文件并读取头部信息
    if (avformat_open_input(&rawFmtCtx, filename.c_str(), nullptr, nullptr) < 0) {
        std::cout << "无法打开输入文件" << std::endl;
        return info;
    }

    AVFormatContextPtr fmtCtx(rawFmtCtx);

    // 检索流信息
    if (avformat_find_stream_info(fmtCtx.get(), nullptr) < 0) {
        std::cout << "无法获取流信息" << std::endl;
        return info;
    }

    // 遍历所有流，找到视频流并获取其 time_base
    for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
        AVStream* videoStream = fmtCtx->streams[i];
        if (videoStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            AVRational timeBase = videoStream->time_base;
            int height = videoStream->codecpar->height;
            int width = videoStream->codecpar->width;
            AVRational sampleAspectRatio = videoStream->sample_aspect_ratio;
            int64_t bitRate = videoStream->codecpar->bit_rate;
            auto pixFmt = static_cast<AVPixelFormat>(videoStream->codecpar->format);
            AVRational framerate = videoStream->r_frame_rate;

            PV("time_base.num", timeBase.num, "time_base.den", timeBase.den, "height", height, "width", width,
               "sample_aspect_ratio.num", sampleAspectRatio.num, "sample_aspect_ratio.den", sampleAspectRatio.den,
               "bit_rate", bitRate, "pix_fmt", pixFmt, "framerate.num", framerate.num, "framerate.den", framerate.den);
            info = {.timeBase = timeBase,
                    .height = height,
                    .width = width,
                    .sampleAspectRatio = sampleAspectRatio,
                    .bitRate = bitRate,
                    .pixFmt = pixFmt,
                    .framerate = framerate};
            break;
        }
    }

    return info;
}

// DecodeAndEncodeAndReplaceMultiYuvs()实现：将输入MP4文件解码，替换指定区间的YUV帧为replaceYuvFiles中的YUV帧，并重新编码为输出MP4文件
// 1. 解码 input_filename，获取所有YUV帧
// 2. 遍历帧序列，将 [toReplaceYuvStartIdx, toReplaceYuvEndIdx] 区间的帧替换为 replaceYuvFiles 中的YUV帧
// 3. 重新编码所有帧为 output_filename
inline void DecodeAndEncodeAndReplaceMultiYuvs(const char* inputFilename, const char* outputFilename,
                                               int toReplaceYuvStartIdx, int toReplaceYuvEndIdx,
                                               const std::vector<std::string>& replaceYuvFiles)
{
    if (!inputFilename || !outputFilename) {
        std::cout << "Invalid input or output filename" << std::endl;
        return;
    }
    if (toReplaceYuvStartIdx <= 0 || toReplaceYuvEndIdx < toReplaceYuvStartIdx) {
        std::cout << "Invalid replace index range: [" << toReplaceYuvStartIdx << ", " << toReplaceYuvEndIdx
                  << "], no replacement will be performed" << std::endl;
    }
    // 修复 #4：校验 replaceYuvFiles 数量与替换区间长度一致
    const int expectedReplaceCount = toReplaceYuvEndIdx - toReplaceYuvStartIdx + 1;
    if (!replaceYuvFiles.empty() && static_cast<int>(replaceYuvFiles.size()) != expectedReplaceCount) {
        std::cout << "Error: replaceYuvFiles.size() = " << replaceYuvFiles.size() << " != range size "
                  << expectedReplaceCount << " ([start=" << toReplaceYuvStartIdx << ", end=" << toReplaceYuvEndIdx
                  << "])" << std::endl;
        return; // ← 直接拒绝，避免静默出错
    }

    ON_SCOPE_EXIT
    {
        CountFrames(inputFilename);
        CountFrames(outputFilename);
    };

    AVFormatContext* rawInputFmt = nullptr;
    if (avformat_open_input(&rawInputFmt, inputFilename, nullptr, nullptr) < 0) {
        std::cout << "Could not open input file: " << inputFilename << std::endl;
        return;
    }
    AVFormatContextPtr inputFmt(rawInputFmt);

    if (avformat_find_stream_info(inputFmt.get(), nullptr) < 0) {
        std::cout << "Failed to retrieve input stream information: " << inputFilename << std::endl;
        return;
    }
    av_dump_format(inputFmt.get(), 0, inputFilename, 0);

    int videoStreamIndex = -1;
    AVCodecParameters* codecpar = nullptr;
    const AVCodec* decoder = nullptr;
    for (unsigned int i = 0; i < inputFmt->nb_streams; ++i) {
        if (inputFmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = static_cast<int>(i);
            codecpar = inputFmt->streams[i]->codecpar;
            decoder = avcodec_find_decoder(codecpar->codec_id);
            break;
        }
    }
    if (videoStreamIndex == -1 || !decoder) {
        std::cout << "Could not find video stream in input: " << inputFilename << std::endl;
        return;
    }

    AVCodecContext* rawDecCtx = avcodec_alloc_context3(decoder);
    if (!rawDecCtx) {
        std::cout << "Failed to allocate decoder context" << std::endl;
        return;
    }
    AVCodecContextPtr decCtx(rawDecCtx);

    if (avcodec_parameters_to_context(decCtx.get(), codecpar) < 0) {
        std::cout << "Failed to copy codec params to decoder context" << std::endl;
        return;
    }
    if (avcodec_open2(decCtx.get(), decoder, nullptr) < 0) {
        std::cout << "Failed to open decoder" << std::endl;
        return;
    }

    AVFormatContext* rawOutFmt = nullptr;
    if (avformat_alloc_output_context2(&rawOutFmt, nullptr, nullptr, outputFilename) < 0 || !rawOutFmt) {
        std::cout << "Could not create output context: " << outputFilename << std::endl;
        return;
    }
    auto outFmtDeleter = [](AVFormatContext* p) {
        if (p) {
            if (!(p->oformat->flags & AVFMT_NOFILE)) {
                avio_closep(&p->pb);
            }
            avformat_free_context(p);
        }
    };
    std::unique_ptr<AVFormatContext, decltype(outFmtDeleter)> outFmt(rawOutFmt, outFmtDeleter);

    AVStream* outStream = avformat_new_stream(outFmt.get(), nullptr);
    if (!outStream) {
        std::cout << "Failed allocating output stream" << std::endl;
        return;
    }

    // 开始
    // 替换原有 `const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);` 之后直到 avcodec_open2(...) 的代码段
    // 使用 libx264（如果可用）并传入质量控制参数（preset / crf / profile）
    const AVCodec* encoder = avcodec_find_encoder_by_name("libx264");
    if (!encoder) {
        // 回退到默认 h264 编码器
        encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    if (!encoder) {
        std::cout << "Encoder not found (H264/libx264)" << std::endl;
        return;
    }

    AVCodecContext* rawEncCtx = avcodec_alloc_context3(encoder);
    if (!rawEncCtx) {
        std::cout << "Failed to allocate encoder context" << std::endl;
        return;
    }
    AVCodecContextPtr encCtx(rawEncCtx);

    // 基本参数（强制 YUV420P 以兼容 libx264）
    auto [timeBase, height, width, sampleAspectRatio, bitRate, pixFmt, framerate] = GetAVStreamTempInfo(inputFilename);
    encCtx->height = height;
    encCtx->width = width;
    encCtx->time_base = timeBase;
    encCtx->framerate = framerate;
    // 使用较高比特率或依赖 CRF 控制质量；这里同时设置一个较高的 bitrate 作为保险
    encCtx->bit_rate = 4000000; // 4 Mbps
    encCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    encCtx->gop_size = 10;
    encCtx->max_b_frames = 1;
    encCtx->codec_id = AV_CODEC_ID_H264;
    encCtx->codec_type = AVMEDIA_TYPE_VIDEO;

    if (outFmt->oformat->flags & AVFMT_GLOBALHEADER) {
        encCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 传入 libx264 特定选项：使用 CRF 控制质量，preset 控制速度/质量
    AVDictionary* encOpts = nullptr;
    // 调整这些值：crf 18 ~ 23 为常用高质量范围；preset 可选 veryfast/medium/slow
    av_dict_set(&encOpts, "preset", "slow", 0);
    av_dict_set(&encOpts, "crf", "20", 0);       // 质量参数，值越小质量越高（并增大文件）
    av_dict_set(&encOpts, "profile", "high", 0); // high profile
    av_dict_set(&encOpts, "tune", "film", 0);    // 可选，根据内容调整
    av_dict_set(&encOpts, "threads", "0", 0);    // 自动线程

    if (avcodec_open2(encCtx.get(), encoder, &encOpts) < 0) {
        std::cout << "Cannot open video encoder (libx264) with options" << std::endl;
        av_dict_free(&encOpts);
        return;
    }
    av_dict_free(&encOpts);

    // 之后仍需调用 avcodec_parameters_from_context(...)

    // 结束
    if (avcodec_parameters_from_context(outStream->codecpar, encCtx.get()) < 0) {
        std::cout << "Failed to copy encoder parameters to output stream" << std::endl;
        return;
    }
    outStream->time_base = encCtx->time_base;

    if (!(outFmt->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outFmt->pb, outputFilename, AVIO_FLAG_WRITE) < 0) {
            std::cout << "Could not open output file: " << outputFilename << std::endl;
            return;
        }
    }

    if (avformat_write_header(outFmt.get(), nullptr) < 0) {
        std::cout << "Error occurred when writing header to output" << std::endl;
        return;
    }

    AVPacket* rawPkt = av_packet_alloc();
    AVPacketPtr pkt(rawPkt);
    AVFrame* rawFrame = av_frame_alloc();
    AVFramePtr frame(rawFrame);
    if (!pkt || !frame) {
        std::cout << "Failed to allocate packet/frame" << std::endl;
        return;
    }

    int response = 0;
    int frameIndex = 1;
    int64_t lastDecodedPts = AV_NOPTS_VALUE;
    bool insertedReplacements = false;

    while (av_read_frame(inputFmt.get(), pkt.get()) >= 0) {
        if (pkt->stream_index == videoStreamIndex) {
            response = avcodec_send_packet(decCtx.get(), pkt.get());
            if (response < 0) {
                std::cout << "Error sending packet to decoder: " << response << std::endl;
                av_packet_unref(pkt.get());
                break;
            }
            while (true) {
                response = avcodec_receive_frame(decCtx.get(), frame.get());
                if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                    break;
                }
                if (response < 0) {
                    std::cout << "Error receiving frame from decoder: " << response << std::endl;
                    return;
                }

                frame->pts = frame->best_effort_timestamp;
                lastDecodedPts = frame->pts;

                if (frameIndex == toReplaceYuvStartIdx && !replaceYuvFiles.empty() && !insertedReplacements) {
                    for (const auto& replacePath : replaceYuvFiles) {
                        FILE* rawFp = nullptr;
#ifdef _WIN32
                        if (fopen_s(&rawFp, replacePath.c_str(), "rb") != 0) {
                            rawFp = nullptr;
                        }
#else
                        rawFp = fopen(replacePath.c_str(), "rb");
#endif
                        FilePtr repFp(rawFp);
                        if (!repFp) {
                            std::cout << "Warning: cannot open replace YUV file: " << replacePath << std::endl;
                            continue;
                        }

                        AVFrame* repFrame = av_frame_alloc();
                        AVFramePtr repFramePtr(repFrame);
                        if (!repFrame) {
                            std::cout << "Warning: av_frame_alloc failed for replace frame: " << replacePath
                                      << std::endl;
                            continue;
                        }
                        repFrame->format = AV_PIX_FMT_YUV420P;
                        repFrame->width = encCtx->width;
                        repFrame->height = encCtx->height;
                        if (av_frame_get_buffer(repFrame, 32) < 0) {
                            std::cout << "Warning: av_frame_get_buffer failed for: " << replacePath << std::endl;
                            continue;
                        }

                        bool readOk = true;
                        for (int y = 0; y < repFrame->height && readOk; ++y) {
                            auto want = static_cast<size_t>(repFrame->width);
                            size_t got = fread(repFrame->data[0] + (static_cast<size_t>(y) * repFrame->linesize[0]), 1,
                                               want, repFp.get());
                            if (got != want) {
                                readOk = false;
                            }
                        }
                        for (int y = 0; y < repFrame->height / 2 && readOk; ++y) {
                            auto want = static_cast<size_t>(repFrame->width / 2);
                            size_t got = fread(repFrame->data[1] + (static_cast<size_t>(y) * repFrame->linesize[1]), 1,
                                               want, repFp.get());
                            if (got != want) {
                                readOk = false;
                            }
                        }
                        for (int y = 0; y < repFrame->height / 2 && readOk; ++y) {
                            auto want = static_cast<size_t>(repFrame->width / 2);
                            size_t got = fread(repFrame->data[2] + (static_cast<size_t>(y) * repFrame->linesize[2]), 1,
                                               want, repFp.get());
                            if (got != want) {
                                readOk = false;
                            }
                        }
                        if (!readOk) {
                            std::cout << "Warning: read YUV failed for: " << replacePath << std::endl;
                            continue;
                        }

                        if (lastDecodedPts == AV_NOPTS_VALUE) {
                            lastDecodedPts = 0;
                        }
                        lastDecodedPts += 1;
                        repFrame->pts = lastDecodedPts;

                        AVFrame* frameToSend = repFrame;
                        AVFrame* convFrame = nullptr;
                        SwsContext* convSws = nullptr;
                        AVFramePtr convFramePtr(nullptr);

                        if (encCtx->pix_fmt != AV_PIX_FMT_YUV420P) {
                            convFrame = av_frame_alloc();
                            convFramePtr.reset(convFrame);
                            if (convFrame) {
                                convFrame->format = encCtx->pix_fmt;
                                convFrame->width = repFrame->width;
                                convFrame->height = repFrame->height;
                                if (av_frame_get_buffer(convFrame, 32) == 0) {
                                    convSws = sws_getContext(repFrame->width, repFrame->height, AV_PIX_FMT_YUV420P,
                                                             repFrame->width, repFrame->height, encCtx->pix_fmt,
                                                             SWS_BILINEAR, nullptr, nullptr, nullptr);
                                    if (convSws) {
                                        sws_scale(convSws, repFrame->data, repFrame->linesize, 0, repFrame->height,
                                                  convFrame->data, convFrame->linesize);
                                        convFrame->pts = repFrame->pts;
                                        frameToSend = convFrame;
                                    }
                                }
                            }
                        }

                        response = avcodec_send_frame(encCtx.get(), frameToSend);
                        if (response < 0) {
                            std::cout << "Error sending replacement frame to encoder: " << response << std::endl;
                        } else {
                            AVPacket* rawOutPkt = av_packet_alloc();
                            AVPacketPtr outPkt(rawOutPkt);
                            while (avcodec_receive_packet(encCtx.get(), outPkt.get()) == 0) {
                                av_packet_rescale_ts(outPkt.get(), encCtx->time_base, outStream->time_base);
                                outPkt->stream_index = outStream->index;
                                av_interleaved_write_frame(outFmt.get(), outPkt.get());
                                av_packet_unref(outPkt.get());
                            }
                        }

                        if (convSws) {
                            sws_freeContext(convSws);
                        }
                    } // end for replace files

                    insertedReplacements = true;
                } // end if reached start

                if (frameIndex >= toReplaceYuvStartIdx && frameIndex <= toReplaceYuvEndIdx) {
                    frameIndex += 1;
                    continue;
                }

                response = avcodec_send_frame(encCtx.get(), frame.get());
                if (response < 0) {
                    std::cout << "Error sending decoded frame to encoder: " << response << std::endl;
                    return;
                }
                AVPacket* rawOutPkt = av_packet_alloc();
                AVPacketPtr outPkt(rawOutPkt);
                while (avcodec_receive_packet(encCtx.get(), outPkt.get()) == 0) {
                    av_packet_rescale_ts(outPkt.get(), encCtx->time_base, outStream->time_base);
                    outPkt->stream_index = outStream->index;
                    av_interleaved_write_frame(outFmt.get(), outPkt.get());
                    av_packet_unref(outPkt.get());
                }

                frameIndex += 1;
            }
        }
        av_packet_unref(pkt.get());
    }

    avcodec_send_packet(decCtx.get(), nullptr);
    while (avcodec_receive_frame(decCtx.get(), frame.get()) == 0) {
        frame->pts = frame->best_effort_timestamp;
        lastDecodedPts = frame->pts;

        if (frameIndex == toReplaceYuvStartIdx && !replaceYuvFiles.empty() && !insertedReplacements) {
            for (const auto& replacePath : replaceYuvFiles) {
                FILE* rawFp = nullptr;
#ifdef _WIN32
                if (fopen_s(&rawFp, replacePath.c_str(), "rb") != 0) {
                    rawFp = nullptr;
                }
#else
                rawFp = fopen(replacePath.c_str(), "rb");
#endif
                FilePtr repFp(rawFp);
                if (!repFp) {
                    std::cout << "Warning: cannot open replace YUV file: " << replacePath << std::endl;
                    continue;
                }

                AVFrame* repFrame = av_frame_alloc();
                AVFramePtr repFramePtr(repFrame);
                if (!repFrame) {
                    std::cout << "Warning: av_frame_alloc failed for replace frame: " << replacePath << std::endl;
                    continue;
                }
                repFrame->format = AV_PIX_FMT_YUV420P;
                repFrame->width = encCtx->width;
                repFrame->height = encCtx->height;
                if (av_frame_get_buffer(repFrame, 32) < 0) {
                    std::cout << "Warning: av_frame_get_buffer failed for: " << replacePath << std::endl;
                    continue;
                }

                bool readOk = true;
                for (int y = 0; y < repFrame->height && readOk; ++y) {
                    auto want = static_cast<size_t>(repFrame->width);
                    size_t got =
                        fread(repFrame->data[0] + (static_cast<size_t>(y) * repFrame->linesize[0]), 1, want, repFp.get());
                    if (got != want) {
                        readOk = false;
                    }
                }
                for (int y = 0; y < repFrame->height / 2 && readOk; ++y) {
                    auto want = static_cast<size_t>(repFrame->width / 2);
                    size_t got =
                        fread(repFrame->data[1] + (static_cast<size_t>(y) * repFrame->linesize[1]), 1, want, repFp.get());
                    if (got != want) {
                        readOk = false;
                    }
                }
                for (int y = 0; y < repFrame->height / 2 && readOk; ++y) {
                    auto want = static_cast<size_t>(repFrame->width / 2);
                    size_t got =
                        fread(repFrame->data[2] + (static_cast<size_t>(y) * repFrame->linesize[2]), 1, want, repFp.get());
                    if (got != want) {
                        readOk = false;
                    }
                }
                if (!readOk) {
                    std::cout << "Warning: read YUV failed for: " << replacePath << std::endl;
                    continue;
                }

                if (lastDecodedPts == AV_NOPTS_VALUE) {
                    lastDecodedPts = 0;
                }
                lastDecodedPts += 1;
                repFrame->pts = lastDecodedPts;

                AVFrame* frameToSend = repFrame;
                AVFrame* convFrame = nullptr;
                SwsContext* convSws = nullptr;
                AVFramePtr convFramePtr(nullptr);

                if (encCtx->pix_fmt != AV_PIX_FMT_YUV420P) {
                    convFrame = av_frame_alloc();
                    convFramePtr.reset(convFrame);
                    if (convFrame) {
                        convFrame->format = encCtx->pix_fmt;
                        convFrame->width = repFrame->width;
                        convFrame->height = repFrame->height;
                        if (av_frame_get_buffer(convFrame, 32) == 0) {
                            convSws = sws_getContext(repFrame->width, repFrame->height, AV_PIX_FMT_YUV420P,
                                                     repFrame->width, repFrame->height, encCtx->pix_fmt, SWS_BILINEAR,
                                                     nullptr, nullptr, nullptr);
                            if (convSws) {
                                sws_scale(convSws, repFrame->data, repFrame->linesize, 0, repFrame->height,
                                          convFrame->data, convFrame->linesize);
                                convFrame->pts = repFrame->pts;
                                frameToSend = convFrame;
                            }
                        }
                    }
                }

                response = avcodec_send_frame(encCtx.get(), frameToSend);
                if (response < 0) {
                    std::cout << "Error sending replacement tail frame to encoder: " << response << std::endl;
                } else {
                    AVPacket* rawOutPkt = av_packet_alloc();
                    AVPacketPtr outPkt(rawOutPkt);
                    while (avcodec_receive_packet(encCtx.get(), outPkt.get()) == 0) {
                        av_packet_rescale_ts(outPkt.get(), encCtx->time_base, outStream->time_base);
                        outPkt->stream_index = outStream->index;
                        av_interleaved_write_frame(outFmt.get(), outPkt.get());
                        av_packet_unref(outPkt.get());
                    }
                }

                if (convSws) {
                    sws_freeContext(convSws);
                }
            } // end insert replacements when tail

            insertedReplacements = true;
        }

        if (frameIndex >= toReplaceYuvStartIdx && frameIndex <= toReplaceYuvEndIdx) {
            frameIndex += 1;
            continue;
        }

        response = avcodec_send_frame(encCtx.get(), frame.get());
        if (response < 0) {
            std::cout << "Error sending flushed decoded frame to encoder: " << response << std::endl;
            return;
        }
        AVPacket* rawOutPkt = av_packet_alloc();
        AVPacketPtr outPkt(rawOutPkt);
        while (avcodec_receive_packet(encCtx.get(), outPkt.get()) == 0) {
            av_packet_rescale_ts(outPkt.get(), encCtx->time_base, outStream->time_base);
            outPkt->stream_index = outStream->index;
            av_interleaved_write_frame(outFmt.get(), outPkt.get());
            av_packet_unref(outPkt.get());
        }

        frameIndex += 1;
    }

    if (!insertedReplacements && !replaceYuvFiles.empty() && frameIndex <= toReplaceYuvStartIdx) {
        for (const auto& replacePath : replaceYuvFiles) {
            FILE* rawFp = nullptr;
#ifdef _WIN32
            if (fopen_s(&rawFp, replacePath.c_str(), "rb") != 0) {
                rawFp = nullptr;
            }
#else
            rawFp = fopen(replacePath.c_str(), "rb");
#endif
            FilePtr repFp(rawFp);
            if (!repFp) {
                std::cout << "Warning: cannot open replace YUV file: " << replacePath << std::endl;
                continue;
            }

            AVFrame* repFrame = av_frame_alloc();
            AVFramePtr repFramePtr(repFrame);
            if (!repFrame) {
                std::cout << "Warning: av_frame_alloc failed for replace frame: " << replacePath << std::endl;
                continue;
            }
            repFrame->format = AV_PIX_FMT_YUV420P;
            repFrame->width = encCtx->width;
            repFrame->height = encCtx->height;
            if (av_frame_get_buffer(repFrame, 32) < 0) {
                std::cout << "Warning: av_frame_get_buffer failed for: " << replacePath << std::endl;
                continue;
            }

            bool readOk = true;
            for (int y = 0; y < repFrame->height && readOk; ++y) {
                auto want = static_cast<size_t>(repFrame->width);
                size_t got =
                    fread(repFrame->data[0] + (static_cast<size_t>(y) * repFrame->linesize[0]), 1, want, repFp.get());
                if (got != want) {
                    readOk = false;
                }
            }
            for (int y = 0; y < repFrame->height / 2 && readOk; ++y) {
                auto want = static_cast<size_t>(repFrame->width / 2);
                size_t got =
                    fread(repFrame->data[1] + (static_cast<size_t>(y) * repFrame->linesize[1]), 1, want, repFp.get());
                if (got != want) {
                    readOk = false;
                }
            }
            for (int y = 0; y < repFrame->height / 2 && readOk; ++y) {
                auto want = static_cast<size_t>(repFrame->width / 2);
                size_t got =
                    fread(repFrame->data[2] + (static_cast<size_t>(y) * repFrame->linesize[2]), 1, want, repFp.get());
                if (got != want) {
                    readOk = false;
                }
            }
            if (!readOk) {
                std::cout << "Warning: read YUV failed for: " << replacePath << std::endl;
                continue;
            }

            if (lastDecodedPts == AV_NOPTS_VALUE) {
                lastDecodedPts = 0;
            }
            lastDecodedPts += 1;
            repFrame->pts = lastDecodedPts;

            AVFrame* frameToSend = repFrame;
            AVFrame* convFrame = nullptr;
            SwsContext* convSws = nullptr;
            AVFramePtr convFramePtr(nullptr);

            if (encCtx->pix_fmt != AV_PIX_FMT_YUV420P) {
                convFrame = av_frame_alloc();
                convFramePtr.reset(convFrame);
                if (convFrame) {
                    convFrame->format = encCtx->pix_fmt;
                    convFrame->width = repFrame->width;
                    convFrame->height = repFrame->height;
                    if (av_frame_get_buffer(convFrame, 32) == 0) {
                        convSws =
                            sws_getContext(repFrame->width, repFrame->height, AV_PIX_FMT_YUV420P, repFrame->width,
                                           repFrame->height, encCtx->pix_fmt, SWS_BILINEAR, nullptr, nullptr, nullptr);
                        if (convSws) {
                            sws_scale(convSws, repFrame->data, repFrame->linesize, 0, repFrame->height, convFrame->data,
                                      convFrame->linesize);
                            convFrame->pts = repFrame->pts;
                            frameToSend = convFrame;
                        }
                    }
                }
            }

            response = avcodec_send_frame(encCtx.get(), frameToSend);
            if (response < 0) {
                std::cout << "Error sending appended replacement frame to encoder: " << response << std::endl;
            } else {
                AVPacket* rawOutPkt = av_packet_alloc();
                AVPacketPtr outPkt(rawOutPkt);
                while (avcodec_receive_packet(encCtx.get(), outPkt.get()) == 0) {
                    av_packet_rescale_ts(outPkt.get(), encCtx->time_base, outStream->time_base);
                    outPkt->stream_index = outStream->index;
                    av_interleaved_write_frame(outFmt.get(), outPkt.get());
                    av_packet_unref(outPkt.get());
                }
            }

            if (convSws) {
                sws_freeContext(convSws);
            }
        }
    }

    avcodec_send_frame(encCtx.get(), nullptr);
    while (true) {
        int ret = avcodec_receive_packet(encCtx.get(), pkt.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            std::cout << "Error while receiving packet from encoder during flush: " << ret << std::endl;
            break;
        }
        av_packet_rescale_ts(pkt.get(), encCtx->time_base, outStream->time_base);
        pkt->stream_index = outStream->index;
        av_interleaved_write_frame(outFmt.get(), pkt.get());
        av_packet_unref(pkt.get());
    }

    av_write_trailer(outFmt.get());

    std::cout << "DecodeAndEncodeAndReplaceMultiYuvs finished: " << outputFilename << std::endl;
}

// 将in_filename 做解码，解码的每帧yuv存入 outDir路径下的 1.yuv 2.yuv ...
inline int DecodeMp4ToYuvFrames(const char* inFilename, const std::string& outDir = "")
{
    if (!inFilename) {
        std::cout << "Invalid input filename" << std::endl;
        return -1;
    }

    GetAVStreamTempInfo(inFilename);

    AVFormatContext* rawFmtCtx{};
    if (avformat_open_input(&rawFmtCtx, inFilename, nullptr, nullptr) < 0) {
        std::cout << "无法打开输入文件: " << inFilename << std::endl;
        return -1;
    }
    AVFormatContextPtr fmtCtx(rawFmtCtx);

    if (avformat_find_stream_info(fmtCtx.get(), nullptr) < 0) {
        std::cout << "无法读取流信息" << std::endl;
        return -1;
    }

    // 查找视频流
    int videoStreamIndex{-1};
    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = static_cast<int>(i);
            break;
        }
    }
    if (videoStreamIndex < 0) {
        std::cout << "未找到视频流" << std::endl;
        return -1;
    }

    // 获取 codecpar 并查找解码器
    AVCodecParameters* codecpar = fmtCtx->streams[videoStreamIndex]->codecpar;
    if (!codecpar) {
        std::cout << "流参数为空" << std::endl;
        return -1;
    }
    const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder) {
        std::cout << "找不到解码器" << std::endl;
        return -1;
    }

    // 解码器上下文（RAII）
    AVCodecContext* rawDecCtx = avcodec_alloc_context3(decoder);
    if (!rawDecCtx) {
        std::cout << "无法分配解码器上下文" << std::endl;
        return -1;
    }
    AVCodecContextPtr decCtx(rawDecCtx);

    if (avcodec_parameters_to_context(decCtx.get(), codecpar) < 0) {
        std::cout << "从参数创建解码器上下文失败" << std::endl;
        return -1;
    }
    if (avcodec_open2(decCtx.get(), decoder, nullptr) < 0) {
        std::cout << "打开解码器失败" << std::endl;
        return -1;
    }

    // out_dir 由参数决定：若 outDir 为空字符串则使用当前代码路径，否则使用传入路径
    fs::path outputDir;
    if (outDir.empty()) {
        outputDir = fs::current_path();
    } else {
        outputDir = fs::path(outDir);
        if (outputDir.has_extension() && outputDir.extension() == ".yuv") {
            outputDir = outputDir.parent_path();
        }
        if (outputDir.empty()) {
            outputDir = fs::current_path();
        }
    }

    std::error_code ec;
    if (!fs::exists(outputDir) && !fs::create_directories(outputDir, ec)) {
        if (ec) {
            std::cout << "无法创建输出目录: " << outputDir.string() << ", 错误: " << ec.message() << std::endl;
            return -1;
        }
    }

    // 分配 AVPacket/AVFrame 并使用 RAII 包装
    AVPacket* rawPkt = av_packet_alloc();
    AVPacketPtr pkt(rawPkt);
    AVFrame* rawFrame = av_frame_alloc();
    AVFramePtr frame(rawFrame);
    AVFrame* rawYuv = av_frame_alloc();
    AVFramePtr yuv(rawYuv);

    if (!pkt || !frame || !yuv) {
        std::cout << "内存分配失败" << std::endl;
        return -1;
    }

    // 从 codecpar 获取宽高（避免 magic 0 值）
    int width = codecpar->width;
    int height = codecpar->height;
    if (width <= 0 || height <= 0) {
        std::cout << "无效的宽高： " << width << " x " << height << std::endl;
        return -1;
    }

    Dump(YUV_WH_FILE, width, height);

    // 为目标 YUV420P 帧分配缓冲
    yuv->format = AV_PIX_FMT_YUV420P;
    yuv->width = width;
    yuv->height = height;
    if (av_frame_get_buffer(yuv.get(), 32) < 0) {
        std::cout << "无法分配 yuv 帧缓冲" << std::endl;
        return -1;
    }

    // sws 上下文：若输入格式不是 YUV420P 则用于转换（RAII）
    SwsContext* rawSwsCtx{};
    SwsCtxPtr swsCtx(nullptr);

    // Helper: 将单帧写入单独文件，文件名为 {index}.yuv
    auto writeFrameToFile = [&](AVFrame* outFrame, int idx) -> int {
        fs::path fname = outputDir / (std::to_string(idx) + ".yuv");
        FILE* rawFp2{};
#ifdef _WIN32
        if (fopen_s(&rawFp2, fname.string().c_str(), "wb") != 0) {
            rawFp2 = nullptr;
        }
#else
        rawFp2 = fopen(fname.string().c_str(), "wb");
#endif
        FilePtr fp2(rawFp2);
        if (!fp2) {
            std::cout << "无法打开输出文件: " << fname.string() << std::endl;
            return -1;
        }

        // Y plane
        for (int y = 0; y < height; ++y) {
            if (fwrite(outFrame->data[0] + ((size_t)y * outFrame->linesize[0]), 1, width, fp2.get()) != (size_t)width) {
                std::cout << "写入 Y 平面失败: " << fname.string() << std::endl;
                return -1;
            }
        }
        // U plane
        for (int y = 0; y < height / 2; ++y) {
            if (fwrite(outFrame->data[1] + ((size_t)y * outFrame->linesize[1]), 1, width / 2, fp2.get()) !=
                (size_t)(width / 2)) {
                std::cout << "写入 U 平面失败: " << fname.string() << std::endl;
                return -1;
            }
        }
        // V plane
        for (int y = 0; y < height / 2; ++y) {
            if (fwrite(outFrame->data[2] + ((size_t)y * outFrame->linesize[2]), 1, width / 2, fp2.get()) !=
                (size_t)(width / 2)) {
                std::cout << "写入 V 平面失败: " << fname.string() << std::endl;
                return -1;
            }
        }
        return 0;
    };

    // 可选：MJPEG 用于生成 JPG
    const AVCodec* jpegEncoder = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    AVCodecContext* jpegEncCtx{};
    AVPacket* rawJpegPkt = av_packet_alloc();
    AVPacketPtr jpegPkt{rawJpegPkt};
    AVFrame* jpegConvFrame{};
    SwsContext* jpegSwsCtxRaw{};
    SwsCtxPtr jpegSwsCtx{};

    if (jpegEncoder) {
        jpegEncCtx = avcodec_alloc_context3(jpegEncoder);
        if (jpegEncCtx) {
            // 使用非弃用的 YUV420P，且显式声明 JPEG 的 full range
            jpegEncCtx->pix_fmt =
                AV_PIX_FMT_YUV420P; // 关键点：不要使用已弃用的 AV_PIX_FMT_YUVJ420P 作为 sws 的目标像素格式，
            jpegEncCtx->height = height;
            jpegEncCtx->width = width;
            jpegEncCtx->time_base = AVRational{1, 25};
            jpegEncCtx->codec_type = AVMEDIA_TYPE_VIDEO;
            jpegEncCtx->color_range = AVCOL_RANGE_JPEG;

            // 优化质量参数：使用常见键 "quality" 或 "q"（依赖编译时后端）
            AVDictionary* jpegOpts{};
            // 1..100 quality（libjpeg 风格） —— 优先尝试 quality
            av_dict_set(&jpegOpts, "quality", "100", 0); // 生成220KB的单张.jpg文件（之前是20KB）
            // 作为后备也设置 qscale/q（某些后端识别）
            av_dict_set(&jpegOpts, "q", "2", 0);
            av_dict_set(&jpegOpts, "qmin", "2", 0);
            av_dict_set(&jpegOpts, "qmax", "2", 0);

            if (avcodec_open2(jpegEncCtx, jpegEncoder, &jpegOpts) < 0) {
                std::cout << "无法打开 MJPEG 编码器（JPEG 生成功能将被禁用）" << std::endl;
                avcodec_free_context(&jpegEncCtx);
                jpegEncCtx = nullptr;
            }
            av_dict_free(&jpegOpts);
        }
    }

    int frameIndex = 1;

    // 读取并解码每个包
    while (av_read_frame(fmtCtx.get(), pkt.get()) >= 0) {
        if (pkt->stream_index == videoStreamIndex) {
            if (avcodec_send_packet(decCtx.get(), pkt.get()) < 0) {
                av_packet_unref(pkt.get());
                continue;
            }
            while (avcodec_receive_frame(decCtx.get(), frame.get()) == 0) {
                AVFrame* outFrame{};
                if (decCtx->pix_fmt != AV_PIX_FMT_YUV420P) {
                    if (!swsCtx) {
                        rawSwsCtx = sws_getContext(width, height, decCtx->pix_fmt, width, height, AV_PIX_FMT_YUV420P,
                                                   SWS_BILINEAR, nullptr, nullptr, nullptr);
                        if (!rawSwsCtx) {
                            std::cout << "无法创建 sws 上下文" << std::endl;
                            return -1;
                        }
                        swsCtx.reset(rawSwsCtx);
                    }
                    sws_scale(swsCtx.get(), frame->data, frame->linesize, 0, height, yuv->data, yuv->linesize);
                    outFrame = yuv.get();
                } else {
                    outFrame = frame.get();
                }

                if (writeFrameToFile(outFrame, frameIndex) < 0) {
                    return -1;
                }

                // 生成 JPEG（可选）
                if (jpegEncCtx && jpegPkt) {
                    AVFrame* frameForJpeg{};
                    if (outFrame->format != jpegEncCtx->pix_fmt) {
                        if (!jpegConvFrame) {
                            jpegConvFrame = av_frame_alloc();
                            if (jpegConvFrame) {
                                jpegConvFrame->format = jpegEncCtx->pix_fmt;
                                jpegConvFrame->width = width;
                                jpegConvFrame->height = height;
                                jpegConvFrame->color_range = jpegEncCtx->color_range;
                                if (av_frame_get_buffer(jpegConvFrame, 32) < 0) {
                                    av_frame_free(&jpegConvFrame);
                                    jpegConvFrame = nullptr;
                                }
                            }
                        }
                        if (jpegConvFrame) {
                            if (!jpegSwsCtx) {
                                jpegSwsCtxRaw = sws_getContext(
                                    width, height, static_cast<AVPixelFormat>(outFrame->format), width, height,
                                    jpegEncCtx->pix_fmt, SWS_BILINEAR, nullptr, nullptr, nullptr);
                                if (!jpegSwsCtxRaw) {
                                    std::cout << "无法创建 jpeg sws 上下文" << std::endl;
                                    // 继续，不影响 YUV 写出
                                } else {
                                    jpegSwsCtx.reset(jpegSwsCtxRaw);
                                }
                            }
                            if (jpegSwsCtx) {
                                sws_scale(jpegSwsCtx.get(), outFrame->data, outFrame->linesize, 0, height,
                                          jpegConvFrame->data, jpegConvFrame->linesize);
                                jpegConvFrame->color_range = jpegEncCtx->color_range;
                                frameForJpeg = jpegConvFrame;
                            }
                        }
                    } else {
                        frameForJpeg = outFrame;
                    }

                    if (frameForJpeg) {
                        frameForJpeg->pts = frameIndex;
                        if (avcodec_send_frame(jpegEncCtx, frameForJpeg) == 0) {
                            while (avcodec_receive_packet(jpegEncCtx, jpegPkt.get()) == 0) {
                                fs::path jpgPath = outputDir / (std::to_string(frameIndex) + ".jpg");
                                FILE* jpgFp{};
#ifdef _WIN32
                                if (fopen_s(&jpgFp, jpgPath.string().c_str(), "wb") == 0 && jpgFp) {
#else
                                jpgFp = fopen(jpgPath.string().c_str(), "wb");
                                if (jpgFp) {
#endif
                                    fwrite(jpegPkt->data, 1, jpegPkt->size, jpgFp);
#ifdef _WIN32
                                    fclose(jpgFp);
#else
                                    fclose(jpgFp);
#endif
                                }
                                av_packet_unref(jpegPkt.get());
                                break;
                            }
                        }
                    }
                }

                ++frameIndex;
            }
        }
        av_packet_unref(pkt.get());
    }

    // 发送 NULL 刷新解码器缓冲
    avcodec_send_packet(decCtx.get(), nullptr);
    while (avcodec_receive_frame(decCtx.get(), frame.get()) == 0) {
        AVFrame* outFrame{};
        if (decCtx->pix_fmt != AV_PIX_FMT_YUV420P) {
            if (!swsCtx) {
                rawSwsCtx = sws_getContext(width, height, decCtx->pix_fmt, width, height, AV_PIX_FMT_YUV420P,
                                           SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!rawSwsCtx) {
                    std::cout << "无法创建 sws 上下文" << std::endl;
                    return -1;
                }
                swsCtx.reset(rawSwsCtx);
            }
            sws_scale(swsCtx.get(), frame->data, frame->linesize, 0, height, yuv->data, yuv->linesize);
            outFrame = yuv.get();
        } else {
            outFrame = frame.get();
        }

        if (writeFrameToFile(outFrame, frameIndex) < 0) {
            return -1;
        }

        // 也为刷新阶段的帧生成 JPEG（与上文相同的逻辑）
        if (jpegEncCtx && jpegPkt) {
            AVFrame* frameForJpeg{};
            if (outFrame->format != jpegEncCtx->pix_fmt) {
                if (jpegConvFrame && jpegSwsCtx) {
                    sws_scale(jpegSwsCtx.get(), outFrame->data, outFrame->linesize, 0, height, jpegConvFrame->data,
                              jpegConvFrame->linesize);
                    frameForJpeg = jpegConvFrame;
                }
            } else {
                frameForJpeg = outFrame;
            }

            if (frameForJpeg) {
                frameForJpeg->pts = 0;
                if (avcodec_send_frame(jpegEncCtx, frameForJpeg) == 0) {
                    while (avcodec_receive_packet(jpegEncCtx, jpegPkt.get()) == 0) {
                        fs::path jpgPath = outputDir / (std::to_string(frameIndex) + ".jpg");
                        FILE* jpgFp{};
#ifdef _WIN32
                        if (fopen_s(&jpgFp, jpgPath.string().c_str(), "wb") == 0 && jpgFp) {
#else
                        jpgFp = fopen(jpgPath.string().c_str(), "wb");
                        if (jpgFp) {
#endif
                            fwrite(jpegPkt->data, 1, jpegPkt->size, jpgFp);
#ifdef _WIN32
                            fclose(jpgFp);
#else
                            fclose(jpgFp);
#endif
                        }
                        av_packet_unref(jpegPkt.get());
                        break;
                    }
                }
            }
        }

        ++frameIndex;
    }

    // flush jpeg encoder
    if (jpegEncCtx && jpegPkt) {
        avcodec_send_frame(jpegEncCtx, nullptr);
        while (avcodec_receive_packet(jpegEncCtx, jpegPkt.get()) == 0) {
            fs::path jpgPath = outputDir / (std::to_string(frameIndex) + ".jpg");
            FILE* jpgFp{};
#ifdef _WIN32
            if (fopen_s(&jpgFp, jpgPath.string().c_str(), "wb") == 0 && jpgFp) {
#else
            jpgFp = fopen(jpgPath.string().c_str(), "wb");
            if (jpgFp) {
#endif
                fwrite(jpegPkt->data, 1, jpegPkt->size, jpgFp);
#ifdef _WIN32
                fclose(jpgFp);
#else
                fclose(jpgFp);
#endif
            }
            av_packet_unref(jpegPkt.get());
            ++frameIndex;
        }
    }

    // 释放 JPEG 相关资源（若使用）
    if (jpegSwsCtx) {
        sws_freeContext(jpegSwsCtx.release());
    }
    if (jpegConvFrame) {
        av_frame_free(&jpegConvFrame);
    }
    if (jpegPkt) {
        // jpeg_pkt 为 AVPacketPtr，会在析构时释放
    }
    if (jpegEncCtx) {
        avcodec_free_context(&jpegEncCtx);
    }

    return 0;
}

inline void DecodeCropEncode(const char* inputFilename, const char* outputFilename, double xCropRatio,
                             double yCropRatio)
{
    if (!inputFilename || !outputFilename) {
        std::cout << "Invalid input or output filename" << std::endl;
        return;
    }
    if (xCropRatio < 0.0 || xCropRatio >= 1.0 || yCropRatio < 0.0 || yCropRatio >= 1.0) {
        std::cout << "Invalid crop ratios. Must be in [0,1)." << std::endl;
        return;
    }

    ON_SCOPE_EXIT
    {
        CountFrames(inputFilename);
        CountFrames(outputFilename);
    };

    // 打开输入格式上下文（RAII）
    AVFormatContext* rawInFmt = nullptr;
    if (avformat_open_input(&rawInFmt, inputFilename, nullptr, nullptr) < 0) {
        std::cout << "Could not open input file 『" << inputFilename << "』" << std::endl;
        return;
    }
    AVFormatContextPtr inFmtPtr(rawInFmt);

    if (avformat_find_stream_info(inFmtPtr.get(), nullptr) < 0) {
        std::cout << "Failed to retrieve input stream information" << std::endl;
        return;
    }

    av_dump_format(inFmtPtr.get(), 0, inputFilename, false);

    // 查找视频流和解码器
    int videoStreamIndex = -1;
    AVCodecParameters* codecParams = nullptr;
    const AVCodec* decoder = nullptr;
    for (unsigned int i = 0; i < inFmtPtr->nb_streams; ++i) {
        AVCodecParameters* p = inFmtPtr->streams[i]->codecpar;
        if (p->codec_type == AVMEDIA_TYPE_VIDEO) {
            decoder = avcodec_find_decoder(p->codec_id);
            if (decoder) {
                videoStreamIndex = static_cast<int>(i);
                codecParams = p;
                break;
            }
        }
    }
    if (videoStreamIndex == -1 || !codecParams || !decoder) {
        std::cout << "Could not find video stream in the input, aborting" << std::endl;
        return;
    }

    // 解码器上下文（RAII）
    AVCodecContext* rawDecCtx = avcodec_alloc_context3(decoder);
    if (!rawDecCtx) {
        std::cout << "Failed to allocate memory for AVCodecContext" << std::endl;
        return;
    }
    AVCodecContextPtr decCtxPtr(rawDecCtx);
    if (avcodec_parameters_to_context(decCtxPtr.get(), codecParams) < 0) {
        std::cout << "Failed to copy codec params to codec context" << std::endl;
        return;
    }
    if (avcodec_open2(decCtxPtr.get(), decoder, nullptr) < 0) {
        std::cout << "Failed to open decoder" << std::endl;
        return;
    }

    // 获取流信息用于裁剪计算
    auto [timeBase, orig_h, orig_w, sampleAspectRatio, bitRate, orig_pix_fmt, framerate] =
        GetAVStreamTempInfo(inputFilename);
    if (orig_w <= 0 || orig_h <= 0) {
        std::cout << "Invalid source resolution" << std::endl;
        return;
    }

    // 计算裁剪区域（中心不变）
    int cropLeft = static_cast<int>(std::floor(orig_w * (xCropRatio / 2.0)));
    int cropTop = static_cast<int>(std::floor(orig_h * (yCropRatio / 2.0)));
    int newW = orig_w - (2 * cropLeft);
    int newH = orig_h - (2 * cropTop);
    if (newW <= 0 || newH <= 0) {
        std::cout << "Crop size invalid after applying ratios" << std::endl;
        return;
    }

    // 创建输出格式上下文（自定义 deleter 以正确关闭 pb 并释放 context）
    AVFormatContext* rawOutFmt = nullptr;
    if (avformat_alloc_output_context2(&rawOutFmt, nullptr, nullptr, outputFilename) < 0 || !rawOutFmt) {
        std::cout << "Could not create output context" << std::endl;
        return;
    }
    auto outFmtDeleter = [](AVFormatContext* p) {
        if (p) {
            if (!(p->oformat->flags & AVFMT_NOFILE)) {
                avio_closep(&p->pb);
            }
            avformat_free_context(p);
        }
    };
    std::unique_ptr<AVFormatContext, decltype(outFmtDeleter)> outFmtPtr(rawOutFmt, outFmtDeleter);

    AVStream* outStream = avformat_new_stream(outFmtPtr.get(), nullptr);
    if (!outStream) {
        std::cout << "Failed allocating output stream" << std::endl;
        return;
    }

    // 选择编码器并创建编码器上下文（RAII）
    const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder) {
        std::cout << "Necessary encoder not found" << std::endl;
        return;
    }
    AVCodecContext* rawEncCtx = avcodec_alloc_context3(encoder);
    if (!rawEncCtx) {
        std::cout << "Failed to allocate the encoder context" << std::endl;
        return;
    }
    AVCodecContextPtr encCtxPtr(rawEncCtx);

    // 复制输入参数到 encoder_context 然后覆盖宽高/像素格式/比特率等
    if (avcodec_parameters_to_context(encCtxPtr.get(), codecParams) < 0) {
        std::cout << "Failed to copy codec params to encoder_context " << std::endl;
        return;
    }
    encCtxPtr->width = newW;
    encCtxPtr->height = newH;
    encCtxPtr->time_base = timeBase;
    encCtxPtr->framerate = framerate;
    encCtxPtr->sample_aspect_ratio = sampleAspectRatio;
    encCtxPtr->bit_rate = bitRate > 0 ? bitRate : 400000;
    encCtxPtr->pix_fmt = orig_pix_fmt; // 保持原始格式，若编码器不支持则会转换
    encCtxPtr->codec_id = AV_CODEC_ID_H264;
    encCtxPtr->codec_type = AVMEDIA_TYPE_VIDEO;
    encCtxPtr->gop_size = 10;
    encCtxPtr->max_b_frames = 1;

    if (outFmtPtr->oformat->flags & AVFMT_GLOBALHEADER) {
        encCtxPtr->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    if (avcodec_open2(encCtxPtr.get(), encoder, nullptr) < 0) {
        std::cout << "Cannot open video encoder for stream" << std::endl;
        return;
    }

    if (avcodec_parameters_from_context(outStream->codecpar, encCtxPtr.get()) < 0) {
        std::cout << "Failed to copy encoder parameters to output stream" << std::endl;
        return;
    }
    outStream->time_base = encCtxPtr->time_base;

    if (!(outFmtPtr->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outFmtPtr->pb, outputFilename, AVIO_FLAG_WRITE) < 0) {
            std::cout << "Could not open output file 『" << outputFilename << "』" << std::endl;
            return;
        }
    }

    if (avformat_write_header(outFmtPtr.get(), nullptr) < 0) {
        std::cout << "Error occurred when opening output file" << std::endl;
        return;
    }

    // 分配并用 RAII 包装 packet/frame
    AVPacket* rawPkt = av_packet_alloc();
    AVPacketPtr pktPtr(rawPkt);
    AVFrame* rawDecoded = av_frame_alloc();
    AVFramePtr decodedPtr(rawDecoded);
    AVFrame* rawYuvFull = av_frame_alloc();
    AVFramePtr yuvFullPtr(rawYuvFull);
    AVFrame* rawCropped = av_frame_alloc();
    AVFramePtr croppedPtr(rawCropped);
    AVFrame* rawConvEnc = av_frame_alloc();
    AVFramePtr convEncPtr(rawConvEnc);

    if (!pktPtr || !decodedPtr || !yuvFullPtr || !croppedPtr || !convEncPtr) {
        std::cout << "Allocation failed" << std::endl;
        return;
    }

    // 为 yuv_full 分配 YUV420P 缓冲（full-size）
    yuvFullPtr->format = AV_PIX_FMT_YUV420P;
    yuvFullPtr->width = orig_w;
    yuvFullPtr->height = orig_h;
    if (av_frame_get_buffer(yuvFullPtr.get(), 32) < 0) {
        std::cout << "Failed to allocate yuv_full buffer" << std::endl;
        return;
    }

    // 为 cropped 分配 YUV420P 缓冲（new size）
    croppedPtr->format = AV_PIX_FMT_YUV420P;
    croppedPtr->width = newW;
    croppedPtr->height = newH;
    if (av_frame_get_buffer(croppedPtr.get(), 32) < 0) {
        std::cout << "Failed to allocate cropped buffer" << std::endl;
        return;
    }

    // 为 conv_for_enc 分配缓冲（当编码器像素格式 != YUV420P 时）
    bool encNeedsConv = (encCtxPtr->pix_fmt != AV_PIX_FMT_YUV420P);
    convEncPtr->format = encCtxPtr->pix_fmt;
    convEncPtr->width = newW;
    convEncPtr->height = newH;
    if (encNeedsConv) {
        if (av_frame_get_buffer(convEncPtr.get(), 32) < 0) {
            std::cout << "Failed to allocate conv_for_enc buffer" << std::endl;
            return;
        }
    }

    // sws 上下文使用 RAII
    SwsCtxPtr swsToYuv(nullptr);
    SwsCtxPtr swsToEnc(nullptr);

    // 主循环：读取包 -> 解码 -> 转为 YUV420P(full) -> 中心裁剪 -> （必要时）转换为 encoder pixfmt -> 编码写出
    while (av_read_frame(inFmtPtr.get(), pktPtr.get()) >= 0) {
        if (pktPtr->stream_index != videoStreamIndex) {
            av_packet_unref(pktPtr.get());
            continue;
        }

        int ret = avcodec_send_packet(decCtxPtr.get(), pktPtr.get());
        av_packet_unref(pktPtr.get());
        if (ret < 0) {
            std::cout << "Error while sending a packet to the decoder" << std::endl;
            return;
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(decCtxPtr.get(), decodedPtr.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                std::cout << "Error while receiving a frame from the decoder" << std::endl;
                return;
            }

            decodedPtr->pts = decodedPtr->best_effort_timestamp;

            // 转为 YUV420P full-size（如需）
            if (decodedPtr->format != AV_PIX_FMT_YUV420P || decodedPtr->width != orig_w ||
                decodedPtr->height != orig_h) {
                if (!swsToYuv) {
                    SwsContext* s = sws_getContext(decodedPtr->width, decodedPtr->height,
                                                   static_cast<AVPixelFormat>(decodedPtr->format), orig_w, orig_h,
                                                   AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
                    if (!s) {
                        std::cout << "Failed to create sws_to_yuv" << std::endl;
                        return;
                    }
                    swsToYuv.reset(s);
                }
                sws_scale(swsToYuv.get(), decodedPtr->data, decodedPtr->linesize, 0, decodedPtr->height,
                          yuvFullPtr->data, yuvFullPtr->linesize);
            } else {
                // 逐平面按行拷贝
                for (int p = 0; p < 3; ++p) {
                    int planeH = (p == 0) ? orig_h : (orig_h / 2);
                    int planeW = (p == 0) ? orig_w : (orig_w / 2);
                    for (int r = 0; r < planeH; ++r) {
                        memcpy(yuvFullPtr->data[p] + ((size_t)r * yuvFullPtr->linesize[p]),
                               decodedPtr->data[p] + ((size_t)r * decodedPtr->linesize[p]), static_cast<size_t>(planeW));
                    }
                }
            }

            // 在 YUV420P full 上执行中心裁剪 -> cropped
            for (int r = 0; r < newH; ++r) {
                uint8_t* dst = croppedPtr->data[0] + ((size_t)r * croppedPtr->linesize[0]);
                uint8_t* src = yuvFullPtr->data[0] + ((size_t)(r + cropTop) * yuvFullPtr->linesize[0]) + cropLeft;
                memcpy(dst, src, static_cast<size_t>(newW));
            }
            int cropLeftChroma = cropLeft / 2;
            int cropTopChroma = cropTop / 2;
            int newWChroma = newW / 2;
            int newHChroma = newH / 2;
            for (int r = 0; r < newHChroma; ++r) {
                uint8_t* dstu = croppedPtr->data[1] + ((size_t)r * croppedPtr->linesize[1]);
                uint8_t* srcu =
                    yuvFullPtr->data[1] + ((size_t)(r + cropTopChroma) * yuvFullPtr->linesize[1]) + cropLeftChroma;
                memcpy(dstu, srcu, static_cast<size_t>(newWChroma));
            }
            for (int r = 0; r < newHChroma; ++r) {
                uint8_t* dstv = croppedPtr->data[2] + ((size_t)r * croppedPtr->linesize[2]);
                uint8_t* srcv =
                    yuvFullPtr->data[2] + ((size_t)(r + cropTopChroma) * yuvFullPtr->linesize[2]) + cropLeftChroma;
                memcpy(dstv, srcv, static_cast<size_t>(newWChroma));
            }

            croppedPtr->pts = decodedPtr->pts;

            // 如果编码器像素格式不是 YUV420P，需要转换
            AVFrame* frameToSend = croppedPtr.get();
            if (encNeedsConv) {
                if (!swsToEnc) {
                    SwsContext* s = sws_getContext(newW, newH, AV_PIX_FMT_YUV420P, newW, newH, encCtxPtr->pix_fmt,
                                                   SWS_BILINEAR, nullptr, nullptr, nullptr);
                    if (!s) {
                        std::cout << "Failed to create sws_to_enc" << std::endl;
                        return;
                    }
                    swsToEnc.reset(s);
                }
                sws_scale(swsToEnc.get(), croppedPtr->data, croppedPtr->linesize, 0, newH, convEncPtr->data,
                          convEncPtr->linesize);
                convEncPtr->pts = croppedPtr->pts;
                frameToSend = convEncPtr.get();
            }

            // 发送并接收编码器输出
            if (avcodec_send_frame(encCtxPtr.get(), frameToSend) < 0) {
                std::cout << "Error while sending a frame to the encoder" << std::endl;
                return;
            }
            while (true) {
                int recvRet = avcodec_receive_packet(encCtxPtr.get(), pktPtr.get());
                if (recvRet == AVERROR(EAGAIN) || recvRet == AVERROR_EOF) {
                    break;
                }
                if (recvRet < 0) {
                    std::cout << "Error while receiving a packet from the encoder" << std::endl;
                    return;
                }
                av_packet_rescale_ts(pktPtr.get(), encCtxPtr->time_base, outStream->time_base);
                pktPtr->stream_index = outStream->index;
                if (av_interleaved_write_frame(outFmtPtr.get(), pktPtr.get()) < 0) {
                    std::cout << "Error while writing packet to output file" << std::endl;
                    av_packet_unref(pktPtr.get());
                    return;
                }
                av_packet_unref(pktPtr.get());
            }
        } // end receive_frame loop
    } // end read_frame loop

    // flush decoder
    avcodec_send_packet(decCtxPtr.get(), nullptr);
    while (avcodec_receive_frame(decCtxPtr.get(), decodedPtr.get()) >= 0) {
        decodedPtr->pts = decodedPtr->best_effort_timestamp;

        // 转为 YUV420P full-size（与上文相同）
        if (decodedPtr->format != AV_PIX_FMT_YUV420P || decodedPtr->width != orig_w || decodedPtr->height != orig_h) {
            if (!swsToYuv) {
                SwsContext* s = sws_getContext(decodedPtr->width, decodedPtr->height,
                                               static_cast<AVPixelFormat>(decodedPtr->format), orig_w, orig_h,
                                               AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!s) {
                    std::cout << "Failed to create sws_to_yuv" << std::endl;
                    return;
                }
                swsToYuv.reset(s);
            }
            sws_scale(swsToYuv.get(), decodedPtr->data, decodedPtr->linesize, 0, decodedPtr->height, yuvFullPtr->data,
                      yuvFullPtr->linesize);
        } else {
            for (int p = 0; p < 3; ++p) {
                int planeH = (p == 0) ? orig_h : (orig_h / 2);
                int planeW = (p == 0) ? orig_w : (orig_w / 2);
                for (int r = 0; r < planeH; ++r) {
                    memcpy(yuvFullPtr->data[p] + ((size_t)r * yuvFullPtr->linesize[p]),
                           decodedPtr->data[p] + ((size_t)r * decodedPtr->linesize[p]), static_cast<size_t>(planeW));
                }
            }
        }

        // 裁剪到 cropped
        for (int r = 0; r < newH; ++r) {
            uint8_t* dst = croppedPtr->data[0] + ((size_t)r * croppedPtr->linesize[0]);
            uint8_t* src = yuvFullPtr->data[0] + ((size_t)(r + cropTop) * yuvFullPtr->linesize[0]) + cropLeft;
            memcpy(dst, src, static_cast<size_t>(newW));
        }
        int cropLeftCh = cropLeft / 2;
        int cropTopCh = cropTop / 2;
        int newWCh = newW / 2;
        int newHCh = newH / 2;
        for (int r = 0; r < newHCh; ++r) {
            uint8_t* dstu = croppedPtr->data[1] + ((size_t)r * croppedPtr->linesize[1]);
            uint8_t* srcu = yuvFullPtr->data[1] + ((size_t)(r + cropTopCh) * yuvFullPtr->linesize[1]) + cropLeftCh;
            memcpy(dstu, srcu, static_cast<size_t>(newWCh));
        }
        for (int r = 0; r < newHCh; ++r) {
            uint8_t* dstv = croppedPtr->data[2] + ((size_t)r * croppedPtr->linesize[2]);
            uint8_t* srcv = yuvFullPtr->data[2] + ((size_t)(r + cropTopCh) * yuvFullPtr->linesize[2]) + cropLeftCh;
            memcpy(dstv, srcv, static_cast<size_t>(newWCh));
        }

        croppedPtr->pts = decodedPtr->pts;

        AVFrame* frameToSend = croppedPtr.get();
        if (encNeedsConv) {
            if (!swsToEnc) {
                SwsContext* s = sws_getContext(newW, newH, AV_PIX_FMT_YUV420P, newW, newH, encCtxPtr->pix_fmt,
                                               SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!s) {
                    std::cout << "Failed to create sws_to_enc" << std::endl;
                    return;
                }
                swsToEnc.reset(s);
            }
            sws_scale(swsToEnc.get(), croppedPtr->data, croppedPtr->linesize, 0, newH, convEncPtr->data,
                      convEncPtr->linesize);
            convEncPtr->pts = croppedPtr->pts;
            frameToSend = convEncPtr.get();
        }

        if (avcodec_send_frame(encCtxPtr.get(), frameToSend) < 0) {
            std::cout << "Error while sending a frame to the encoder during flush" << std::endl;
            return;
        }
        while (avcodec_receive_packet(encCtxPtr.get(), pktPtr.get()) == 0) {
            av_packet_rescale_ts(pktPtr.get(), encCtxPtr->time_base, outStream->time_base);
            pktPtr->stream_index = outStream->index;
            av_interleaved_write_frame(outFmtPtr.get(), pktPtr.get());
            av_packet_unref(pktPtr.get());
        }
    }

    // flush encoder
    avcodec_send_frame(encCtxPtr.get(), nullptr);
    while (avcodec_receive_packet(encCtxPtr.get(), pktPtr.get()) >= 0) {
        av_packet_rescale_ts(pktPtr.get(), encCtxPtr->time_base, outStream->time_base);
        pktPtr->stream_index = outStream->index;
        av_interleaved_write_frame(outFmtPtr.get(), pktPtr.get());
        av_packet_unref(pktPtr.get());
    }

    av_write_trailer(outFmtPtr.get());

    // 自动清理由 RAII 处理
}

#endif // INC_LIVEPHOTOSMOOTHPLAY_DECANDENC_H
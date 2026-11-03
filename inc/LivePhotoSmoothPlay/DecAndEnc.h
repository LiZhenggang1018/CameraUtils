/*
 * @Description: 用于执行FFmpeg编解码操作。且编码时根据使用场景决定在视频结尾插入封面帧 或者 FOV过渡帧
 */

#ifndef DECANDENC_H_
#define DECANDENC_H_

// 用于执行FFmpeg编解码操作。且编码时根据使用场景决定在视频结尾插入封面帧 或者 FOV过渡帧
#include <memory>
#include <filesystem>
#include <iostream>

#include "FFmpegDefine.h"

namespace fs = std::filesystem;

struct AVStreamTempInfo
{
	AVRational time_base{};
	int height{};
	int width{};
	AVRational sample_aspect_ratio{};
	int64_t bit_rate{};
	AVPixelFormat pix_fmt{};
	AVRational framerate{};
};

// 获取视频流的临时信息辅助函数，如time_base、分辨率、像素格式等
inline AVStreamTempInfo GetAVStreamTempInfo(const std::string &filename)
{
	AVFormatContext *raw_fmt_ctx{};
	AVStreamTempInfo info{};

	// 打开输入文件并读取头部信息
	if (avformat_open_input(&raw_fmt_ctx, filename.c_str(), nullptr, nullptr) < 0)
	{
		std::cout << "无法打开输入文件" << std::endl;
		return info;
	}

    AVFormatContextPtr fmt_ctx(raw_fmt_ctx);


	// 检索流信息
	if (avformat_find_stream_info(fmt_ctx.get(), nullptr) < 0)
	{
		std::cout << "无法获取流信息" << std::endl;
		return info;
	}

	// 遍历所有流，找到视频流并获取其 time_base
	for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++)
	{
		AVStream *videoStream = fmt_ctx->streams[i];
		if (videoStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			AVRational time_base = videoStream->time_base;
			int height = videoStream->codecpar->height;
			int width = videoStream->codecpar->width;
			AVRational sample_aspect_ratio = videoStream->sample_aspect_ratio;
			int64_t bit_rate = videoStream->codecpar->bit_rate;
			enum AVPixelFormat pix_fmt = static_cast<AVPixelFormat>(videoStream->codecpar->format);
			AVRational framerate = videoStream->r_frame_rate;

			PV("time_base.num", time_base.num, "time_base.den", time_base.den, "height", height, "width", width, "sample_aspect_ratio.num",
			   sample_aspect_ratio.num, "sample_aspect_ratio.den", sample_aspect_ratio.den, "bit_rate", bit_rate, "pix_fmt", pix_fmt, "framerate.num", framerate.num, "framerate.den", framerate.den);
			info = {time_base, height, width, sample_aspect_ratio, bit_rate, pix_fmt, framerate};
			break;
		}
	}

	return info;
}

// 将input_filename解码，编码，然后在视频尾部插入一帧YUV图像（LP封面帧），保存为output_filename。用于模拟生成带有封面帧的LP文件。
inline void DecodeEncodeAndInsertSingleYuv(const char *input_filename, const char *output_filename)
{
	ON_SCOPE_EXIT
	{
		CountFrames(input_filename);
		CountFrames(output_filename);
	};
	AVFormatContext *input_format_context{};
	if (avformat_open_input(&input_format_context, input_filename, nullptr, nullptr) < 0)
	{
		std::cout << "Could not open input file 『" << input_filename << "』" << std::endl;
		return;
	}

	if (avformat_find_stream_info(input_format_context, nullptr) < 0)
	{
		std::cout << "Failed to retrieve input stream information" << std::endl;
		return;
	}

	av_dump_format(input_format_context, 0, input_filename, false);

	const AVCodec *decoder{};
	AVCodecParameters *codec_params{};
	int video_stream_index{-1};
	for (unsigned int i = 0; i < input_format_context->nb_streams; i++)
	{
		AVCodecParameters *local_codec_params = input_format_context->streams[i]->codecpar;
		const AVCodec *local_codec = avcodec_find_decoder(local_codec_params->codec_id);

		if (local_codec == nullptr)
		{
			continue;
		}

		if (local_codec_params->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			video_stream_index = i;
			decoder = local_codec;
			codec_params = local_codec_params;
			break;
		}
	}

	if (video_stream_index == -1)
	{
		std::cout << "Could not find video stream in the input, aborting" << std::endl;
		return;
	}

	AVCodecContext *decoder_context{}; // 值初始化
	decoder_context = avcodec_alloc_context3(decoder);
	if (!decoder_context)
	{
		std::cout << "Failed to allocate memory for AVCodecContext" << std::endl;
		return;
	}

	if (avcodec_parameters_to_context(decoder_context, codec_params) < 0)
	{
		std::cout << "Failed to copy codec params to codec context" << std::endl;
		return;
	}

	if (avcodec_open2(decoder_context, decoder, nullptr) < 0)
	{
		std::cout << "Failed to open codec through avcodec_open2" << std::endl;
		return;
	}

	AVFormatContext *output_format_context{};
	avformat_alloc_output_context2(&output_format_context, nullptr, nullptr, output_filename);
	if (!output_format_context)
	{
		std::cout << "Could not create output context" << std::endl;

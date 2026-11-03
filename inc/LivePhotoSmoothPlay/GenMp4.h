/*
 * @Description: demo展示：生成300帧yuv图像内容，然后通过H.264编码器将这些yuv图像全部编入MP4文件，且帧率为30fps
 */

#ifndef GENMP4_H_
#define GENMP4_H_

#include "FFmpegDefine.h"

// 对于LP的demo暂时不使用此文件。

// 定义图像宽度、高度、帧数和帧率
constexpr int WIDTH = 640;
constexpr int HEIGHT = 480;
constexpr int FRAME_COUNT = 300;
constexpr int FPS = 30;

// 生成简单的 YUV 图像帧
inline void GenSimpleYuvFrames(uint8_t *yuv_data, int frame_index)
{
	// 生成 Y 分量
	for (int y = 0; y < HEIGHT; ++y)
	{
		for (int x = 0; x < WIDTH; ++x)
		{
			yuv_data[y * WIDTH + x] = (x + frame_index) % 256; // Y
		}
	}
	// 生成 U 和 V 分量
	for (int y = 0; y < HEIGHT / 2; ++y)
	{
		for (int x = 0; x < WIDTH / 2; ++x)
		{
			yuv_data[WIDTH * HEIGHT + y * (WIDTH / 2) + x] = 128;							   // U
			yuv_data[WIDTH * HEIGHT + (WIDTH / 2) * (HEIGHT / 2) + y * (WIDTH / 2) + x] = 128; // V
		}
	}
}

inline int GenSimpleMp4()
{
	// 声明 FFmpeg 相关变量
	AVFormatContext *format_context = nullptr;
	const AVOutputFormat *output_format = nullptr;
	AVStream *video_stream = nullptr;
	AVCodecContext *codec_context = nullptr;
	const AVCodec *codec = nullptr;
	AVFrame *frame = nullptr;
	AVPacket pkt;

	// 分配输出格式上下文
	avformat_alloc_output_context2(&format_context, nullptr, nullptr, "output.MP4");
	if (!format_context)
	{
		std::cerr << "无法创建输出上下文\n";
		return -1;
	}

	// 获取输出格式
	output_format = format_context->oformat;
	// 查找 H.264 编码器
	codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!codec)
	{
		std::cerr << "找不到编码器\n";
		return -1;
	}

	// 创建新的视频流
	video_stream = avformat_new_stream(format_context, codec);
	if (!video_stream)
	{
		std::cerr << "无法创建视频流\n";
		return -1;
	}

	// 分配编码器上下文
	codec_context = avcodec_alloc_context3(codec);
	if (!codec_context)
	{
		std::cerr << "无法分配视频编码器上下文\n";
		return -1;
	}

	// 设置编码器参数

	// gop_size：
	// 定义：gop_size 表示一个 GOP（Group of Pictures）的大小，即两个 I 帧之间的帧数。I 帧是关键帧，包含完整的图像数据。
	// 影响：较大的 gop_size 可以提高压缩效率，但会降低视频的编辑灵活性和错误恢复能力。较小的 gop_size 则会增加文件大小，但有助于更快地进行视频编辑和错误恢复 12.
	// max_b_frames：
	// 定义：max_b_frames 表示在两个非 B 帧之间最多可以插入的 B 帧数量。B 帧是双向预测帧，依赖于前后的帧进行解码。
	// 影响：增加 B 帧数量可以提高压缩效率和视频质量，但会增加编码和解码的复杂度和延迟 12.
	// bit_rate：
	// 定义：bit_rate 表示视频的平均比特率，即每秒钟传输的比特数。
	// 影响：较高的比特率通常会提高视频质量，但也会增加文件大小和带宽需求。较低的比特率则会降低视频质量，但有助于减小文件大小和带宽需求

	codec_context->bit_rate = 400000;
	codec_context->width = WIDTH;
	codec_context->height = HEIGHT;
	codec_context->time_base = {1, FPS};
	codec_context->framerate = {FPS, 1};
	codec_context->gop_size = 10;
	codec_context->max_b_frames = 1;
	codec_context->pix_fmt = AV_PIX_FMT_YUV420P;

	// 如果需要全局头部标志，则设置
	if (format_context->oformat->flags & AVFMT_GLOBALHEADER)
	{
		codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	// 打开编码器
	if (avcodec_open2(codec_context, codec, nullptr) < 0)
	{
		std::cerr << "无法打开编码器\n";
		return -1;
	}

	// 设置视频流的时间基准
	video_stream->time_base = {1, FPS};
	avcodec_parameters_from_context(video_stream->codecpar, codec_context);

	// 打开输出文件
	if (!(output_format->flags & AVFMT_NOFILE))
	{
		if (avio_open(&format_context->pb, "output.MP4", AVIO_FLAG_WRITE) < 0)
		{
			std::cerr << "无法打开输出文件\n";
			return -1;
		}
	}

	// 写入文件头部
	if (avformat_write_header(format_context, nullptr) < 0)
	{
		std::cerr << "打开输出文件时出错\n";
		return -1;
	}

	// 分配视频帧
	frame = av_frame_alloc();
	if (!frame)
	{
		std::cerr << "无法分配视频帧\n";
		return -1;
	}
	frame->format = codec_context->pix_fmt;
	frame->width = codec_context->width;
	frame->height = codec_context->height;


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
		return;
	}

	AVStream *out_stream{};
	out_stream = avformat_new_stream(output_format_context, nullptr);
	if (!out_stream)
	{
		std::cout << "Failed allocating output stream" << std::endl;
		return;
	}

	const AVCodec *encoder{};
	encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!encoder)
	{
		std::cout << "Necessary encoder not found" << std::endl;
		return;
	}

	AVCodecContext *encoder_context{};
	encoder_context = avcodec_alloc_context3(encoder);
	if (!encoder_context)
	{
		std::cout << "Failed to allocate the encoder context" << std::endl;
		return;
	}

	if (avcodec_parameters_to_context(encoder_context, codec_params) < 0)
	{
		std::cout << "Failed to copy codec params to encoder_context " << std::endl;
		return;
	}

	auto [time_base, height, width, sample_aspect_ratio, bit_rate, pix_fmt, framerate] = GetAVStreamTempInfo(input_filename);

	encoder_context->time_base = time_base;
	encoder_context->height = height;
	encoder_context->width = width;
	encoder_context->sample_aspect_ratio = sample_aspect_ratio;
	encoder_context->bit_rate = 400000;
	encoder_context->pix_fmt = pix_fmt;
	encoder_context->framerate = framerate;

	encoder_context->codec_id = AV_CODEC_ID_H264;
	encoder_context->codec_type = AVMEDIA_TYPE_VIDEO;

	encoder_context->gop_size = 10;
	encoder_context->max_b_frames = 1;

	if (output_format_context->oformat->flags & AVFMT_GLOBALHEADER)
	{
		encoder_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	if (avcodec_open2(encoder_context, encoder, nullptr) < 0)
	{
		std::cout << "Cannot open video encoder for stream" << std::endl;
		return;
	}

	if (avcodec_parameters_from_context(out_stream->codecpar, encoder_context) < 0)
	{
		std::cout << "Failed to copy encoder parameters to output stream" << std::endl;
		return;
	}

	out_stream->time_base = encoder_context->time_base;

	if (!(output_format_context->oformat->flags & AVFMT_NOFILE))
	{
		if (avio_open(&output_format_context->pb, output_filename, AVIO_FLAG_WRITE) < 0)
		{
			std::cout << "Could not open output file 『" << output_filename << "』" << std::endl;
			return;
		}
	}

	if (avformat_write_header(output_format_context, nullptr) < 0)
	{
		std::cout << "Error occurred when opening output file" << std::endl;
		return;
	}

	AVFrame *frame{};
	AVPacket *packet{};
	frame = av_frame_alloc();
	packet = av_packet_alloc();
	int response{};

	int av_read_frame_cnt{1};
	int av_interleaved_write_frame_cnt{1};

	int64_t last_decoded_pts = AV_NOPTS_VALUE;

	while (av_read_frame(input_format_context, packet) >= 0)
	{
		bool is_av_interleaved_write_frame_success{};
		PV("av_read_frame_cnt:", av_read_frame_cnt++);
		if (packet->stream_index == video_stream_index)
		{
			response = avcodec_send_packet(decoder_context, packet);
			if (response < 0)
			{
				std::cout << "Error while sending a packet to the decoder" << std::endl;
				break;
			}

			int out_response_pos_cnt{};
			while (response >= 0)
			{
				response = avcodec_receive_frame(decoder_context, frame);
				if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
				{
					PV("   out response==", response);
					break;
				}
				else if (response < 0)
				{
					std::cout << "Error while receiving a frame from the decoder" << std::endl;
					return;
				}

				frame->pts = frame->best_effort_timestamp;
				last_decoded_pts = frame->pts;

				frame->pict_type = AV_PICTURE_TYPE_NONE;

				response = avcodec_send_frame(encoder_context, frame);
				if (response < 0)
				{
					std::cout << "Error while sending a frame to the encoder" << std::endl;
					return;
				}

				int in_response_pos_cnt{};
				while (response >= 0)
				{
					PV("          in_response_pos_cnt:", in_response_pos_cnt++);
					response = avcodec_receive_packet(encoder_context, packet);
					if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
					{
						PV("        inner response==", response);
						break;
					}
					else if (response < 0)
					{
						std::cout << "Error while receiving a packet from the encoder" << std::endl;
						return;
					}
					av_packet_rescale_ts(packet, encoder_context->time_base, out_stream->time_base);
					packet->stream_index = out_stream->index;
					response = av_interleaved_write_frame(output_format_context, packet);
					is_av_interleaved_write_frame_success = true;
					av_interleaved_write_frame_cnt++;
					if (response < 0)
					{
						std::cout << "Error while writing packet to output file" << std::endl;
						return;
					}
					av_packet_unref(packet);
				}
			}
		}
		av_packet_unref(packet);
		if (!is_av_interleaved_write_frame_success)
		{
			PV("av_interleaved_write_frame fails!");
		}
	}

	auto skipFrames = av_read_frame_cnt - av_interleaved_write_frame_cnt;
	PV("total:", av_read_frame_cnt, "; write: ", av_interleaved_write_frame_cnt, "; skip: ", skipFrames);

	// Flush decoder
	avcodec_send_packet(decoder_context, nullptr);
	while (avcodec_receive_frame(decoder_context, frame) >= 0)
	{
		frame->pts = frame->best_effort_timestamp;
		last_decoded_pts = frame->pts;

		avcodec_send_frame(encoder_context, frame);
		while (avcodec_receive_packet(encoder_context, packet) >= 0)
		{
			av_interleaved_write_frame(output_format_context, packet);
			av_packet_unref(packet);
		}
	}

	// 插入封面帧逻辑（保持原样但使用值初始化与 cout）
	{
		const char *yuv_path = "60.yuv";
		FILE *yuv_fp{};
		if (fopen_s(&yuv_fp, yuv_path, "rb") != 0)
		{
			yuv_fp = nullptr;
		}
		if (!yuv_fp)
		{
			std::cout << "警告：无法打开插入帧文件：" << yuv_path << " ，跳过插入" << std::endl;
		}
		else
		{
			int w = encoder_context->width;
			int h = encoder_context->height;
			if (w > 0 && h > 0)
			{
				AVFrame *yuv_frame{};
				yuv_frame = av_frame_alloc();
				if (yuv_frame)
				{
					yuv_frame->format = AV_PIX_FMT_YUV420P;
					yuv_frame->width = w;
					yuv_frame->height = h;
					if (av_frame_get_buffer(yuv_frame, 32) == 0)
					{
						size_t read{};
						for (int yy = 0; yy < h; ++yy)
						{
							read = fread(yuv_frame->data[0] + (size_t)yy * yuv_frame->linesize[0], 1, w, yuv_fp);
							if (read != (size_t)w)
							{
								break;
							}
						}
						for (int yy = 0; yy < h / 2; ++yy)
						{
							read = fread(yuv_frame->data[1] + (size_t)yy * yuv_frame->linesize[1], 1, w / 2, yuv_fp);
							if (read != (size_t)(w / 2))
							{
								break;
							}
						}
						for (int yy = 0; yy < h / 2; ++yy)
						{
							read = fread(yuv_frame->data[2] + (size_t)yy * yuv_frame->linesize[2], 1, w / 2, yuv_fp);
							if (read != (size_t)(w / 2))
							{
								break;
							}
						}

						if (last_decoded_pts == AV_NOPTS_VALUE)
						{
							last_decoded_pts = 0;
						}
						yuv_frame->pts = last_decoded_pts + 1;

						AVFrame *frame_to_send = yuv_frame;
						AVFrame *conv_frame{};
						SwsContext *local_sws{};
						if (encoder_context->pix_fmt != AV_PIX_FMT_YUV420P)
						{
							conv_frame = av_frame_alloc();
							if (conv_frame)
							{
								conv_frame->format = encoder_context->pix_fmt;
								conv_frame->width = w;
								conv_frame->height = h;
								if (av_frame_get_buffer(conv_frame, 32) == 0)
								{
									local_sws = sws_getContext(w, h, AV_PIX_FMT_YUV420P,
															   w, h, encoder_context->pix_fmt,
															   SWS_BILINEAR, nullptr, nullptr, nullptr);
									if (local_sws)
									{
										sws_scale(local_sws,
												  yuv_frame->data, yuv_frame->linesize, 0, h,
												  conv_frame->data, conv_frame->linesize);
										conv_frame->pts = yuv_frame->pts;
										frame_to_send = conv_frame;
									}
								}
							}
						}

						response = avcodec_send_frame(encoder_context, frame_to_send);
						if (response < 0)
						{
							std::cout << "Error while sending synthetic frame to encoder: " << response << std::endl;
						}
						else
						{
							while ((response = avcodec_receive_packet(encoder_context, packet)) >= 0)
							{
								av_packet_rescale_ts(packet, encoder_context->time_base, out_stream->time_base);
								packet->stream_index = out_stream->index;
								av_interleaved_write_frame(output_format_context, packet);
								av_packet_unref(packet);
							}
						}

						if (local_sws)
						{
							sws_freeContext(local_sws);
						}
						if (conv_frame)
						{
							av_frame_free(&conv_frame);
						}
					}
					av_frame_free(&yuv_frame);
				}
			}
			else
			{
				std::cout << "警告：编码器宽高无效，无法插入合成帧" << std::endl;
			}
			fclose(yuv_fp);
		}
	}

	avcodec_send_frame(encoder_context, nullptr);
	while (avcodec_receive_packet(encoder_context, packet) >= 0)
	{
		av_interleaved_write_frame(output_format_context, packet);
		av_packet_unref(packet);
	}

	av_write_trailer(output_format_context);

	avcodec_free_context(&decoder_context);
	avcodec_free_context(&encoder_context);
	avformat_close_input(&input_format_context);
	if (output_format_context && !(output_format_context->oformat->flags & AVFMT_NOFILE))
	{
		avio_closep(&output_format_context->pb);
	}
	avformat_free_context(output_format_context);
	av_frame_free(&frame);
	av_packet_free(&packet);
}

// 将input_filename解码，编码，然后在视频尾部插入多帧YUV图像（LP封面帧），保存为output_filename。用于模拟生成带有封面帧的LP文件。
inline void DecodeAndEncodeAndInsertMultiYuvs(const char *input_filename, const char *output_filename)
{
	ON_SCOPE_EXIT
	{
		CountFrames(input_filename);
		CountFrames(output_filename);
	};
	// 创建输入格式上下文
	AVFormatContext *input_format_context{};
	if (avformat_open_input(&input_format_context, input_filename, nullptr, nullptr) < 0)
	{
		std::cout << "Could not open input file 『" << input_filename << "』" << std::endl;
		return;
	}
	// RAII 管理输入 fmt
	AVFormatContextPtr inputFmtPtr(input_format_context);

	// 获取输入文件的流信息
	if (avformat_find_stream_info(input_format_context, nullptr) < 0)
	{
		std::cout << "Failed to retrieve input stream information" << std::endl;
		return;
	}

	// 输出文件格式和媒体信息
	av_dump_format(input_format_context, 0, input_filename, false);

	// 查找视频流的解码器和相关参数
	const AVCodec *decoder{};
	AVCodecParameters *codec_params{};
	int video_stream_index{-1};
	for (unsigned int i = 0; i < input_format_context->nb_streams; ++i)
	{
		AVCodecParameters *local_codec_params = input_format_context->streams[i]->codecpar;
		const AVCodec *local_codec = avcodec_find_decoder(local_codec_params->codec_id);

		if (local_codec == nullptr)
		{
			continue;
		}

		if (local_codec_params->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			video_stream_index = static_cast<int>(i);
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

	// 创建解码器上下文
	AVCodecContext *decoder_context = avcodec_alloc_context3(decoder);
	if (!decoder_context)
	{
		std::cout << "Failed to allocate memory for AVCodecContext" << std::endl;
		return;
	}
	// RAII 管理 decoder_context
	AVCodecContextPtr decoderCtxPtr(decoder_context);

	// 将解码器参数复制到解码器上下文
	if (avcodec_parameters_to_context(decoder_context, codec_params) < 0)
	{
		std::cout << "Failed to copy codec params to codec context" << std::endl;
		return;
	}

	// 打开解码器
	if (avcodec_open2(decoder_context, decoder, nullptr) < 0)
	{
		std::cout << "Failed to open codec through avcodec_open2" << std::endl;
		return;
	}

	// 创建输出格式上下文
	AVFormatContext *output_format_context{};
	avformat_alloc_output_context2(&output_format_context, nullptr, nullptr, output_filename);
	if (!output_format_context)
	{
		std::cout << "Could not create output context" << std::endl;
		return;
	}
	// 为输出创建专用的 RAII deleter（需要在释放时关闭 pb + free context）
	auto outFmtDeleter = [](AVFormatContext *p)
	{
		if (p)
		{
			if (!(p->oformat->flags & AVFMT_NOFILE))
			{
				avio_closep(&p->pb);
			}
			avformat_free_context(p);
		}
	};
	std::unique_ptr<AVFormatContext, decltype(outFmtDeleter)> outputFmtPtr(output_format_context, outFmtDeleter);

	// 创建输出流
	AVStream *out_stream{};
	out_stream = avformat_new_stream(output_format_context, nullptr);
	if (!out_stream)
	{
		std::cout << "Failed allocating output stream" << std::endl;
		return;
	}

	// 查找编码器
	const AVCodec *encoder{};
	encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!encoder)
	{
		std::cout << "Necessary encoder not found" << std::endl;
		return;
	}

	// 创建编码器上下文
	AVCodecContext *encoder_context{};
	encoder_context = avcodec_alloc_context3(encoder);
	if (!encoder_context)
	{
		std::cout << "Failed to allocate the encoder context" << std::endl;
		return;
	}
	// RAII 管理 encoder_context
	AVCodecContextPtr encoderCtxPtr(encoder_context);

	// 复制 codec params 到 encoder_context
	if (avcodec_parameters_to_context(encoder_context, codec_params) < 0)
	{
		std::cout << "Failed to copy codec params to encoder_context " << std::endl;
		return;
	}

	auto [time_base, height, width, sample_aspect_ratio, bit_rate, pix_fmt, framerate] = GetAVStreamTempInfo(input_filename);

	encoder_context->time_base = time_base;
	encoder_context->height = height;
	encoder_context->width = width;
	encoder_context->sample_aspect_ratio = sample_aspect_ratio;
	encoder_context->bit_rate = 400000;
	encoder_context->pix_fmt = pix_fmt;
	encoder_context->framerate = framerate;

	encoder_context->codec_id = AV_CODEC_ID_H264;
	encoder_context->codec_type = AVMEDIA_TYPE_VIDEO;

	encoder_context->gop_size = 10;
	encoder_context->max_b_frames = 1;

	if (output_format_context->oformat->flags & AVFMT_GLOBALHEADER)
	{
		encoder_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	if (avcodec_open2(encoder_context, encoder, nullptr) < 0)
	{
		std::cout << "Cannot open video encoder for stream" << std::endl;
		return;
	}

	if (avcodec_parameters_from_context(out_stream->codecpar, encoder_context) < 0)
	{
		std::cout << "Failed to copy encoder parameters to output stream" << std::endl;
		return;
	}

	out_stream->time_base = encoder_context->time_base;

	if (!(output_format_context->oformat->flags & AVFMT_NOFILE))
	{
		if (avio_open(&output_format_context->pb, output_filename, AVIO_FLAG_WRITE) < 0)
		{
			std::cout << "Could not open output file 『" << output_filename << "』" << std::endl;
			return;
		}
	}

	if (avformat_write_header(output_format_context, nullptr) < 0)
	{
		std::cout << "Error occurred when opening output file" << std::endl;
		return;
	}

	// 使用 RAII 管理 packet/frame
	AVFrame *raw_frame = av_frame_alloc();
	AVFramePtr framePtr(raw_frame);
	AVPacket *raw_packet = av_packet_alloc();
	AVPacketPtr packetPtr(raw_packet);
	if (!framePtr || !packetPtr)
	{
		std::cout << "Failed to allocate frame or packet" << std::endl;
		return;
	}

	int response{};
	int av_read_frame_cnt{1};
	int av_interleaved_write_frame_cnt{1};
	int64_t last_decoded_pts = AV_NOPTS_VALUE;

	// 主循环（使用 packetPtr/framePtr）
	while (av_read_frame(input_format_context, packetPtr.get()) >= 0)
	{
		bool is_av_interleaved_write_frame_success{};
		PV("av_read_frame_cnt:", av_read_frame_cnt++);
		if (packetPtr->stream_index == video_stream_index)
		{
			response = avcodec_send_packet(decoder_context, packetPtr.get());
			if (response < 0)
			{
				std::cout << "Error while sending a packet to the decoder" << std::endl;
				// 清理：调用 av_packet_unref 以便下次复用（unique_ptr 在作用域结束时释放）
				av_packet_unref(packetPtr.get());
				break;
			}

			int out_response_pos_cnt{};
			while (response >= 0)
			{
				response = avcodec_receive_frame(decoder_context, framePtr.get());
				if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
				{
					PV("   out response==", response);
					break;
				}
				else if (response < 0)
				{
					std::cout << "Error while receiving a frame from the decoder" << std::endl;
					av_packet_unref(packetPtr.get());
					return;
				}

				framePtr->pts = framePtr->best_effort_timestamp;
				last_decoded_pts = framePtr->pts;

				framePtr->pict_type = AV_PICTURE_TYPE_NONE;

				response = avcodec_send_frame(encoder_context, framePtr.get());
				if (response < 0)
				{
					std::cout << "Error while sending a frame to the encoder" << std::endl;
					av_packet_unref(packetPtr.get());
					return;
				}

				int in_response_pos_cnt{};
				while (response >= 0)
				{
					PV("          in_response_pos_cnt:", in_response_pos_cnt++);
					response = avcodec_receive_packet(encoder_context, packetPtr.get());
					if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
					{
						PV("        inner response==", response);
						break;
					}
					else if (response < 0)
					{
						std::cout << "Error while receiving a packet from the encoder" << std::endl;
						av_packet_unref(packetPtr.get());
						return;
					}
					av_packet_rescale_ts(packetPtr.get(), encoder_context->time_base, out_stream->time_base);
					packetPtr->stream_index = out_stream->index;
					response = av_interleaved_write_frame(output_format_context, packetPtr.get());
					is_av_interleaved_write_frame_success = true;
					av_interleaved_write_frame_cnt++;
					if (response < 0)
					{
						std::cout << "Error while writing packet to output file" << std::endl;
						av_packet_unref(packetPtr.get());
						return;
					}
					av_packet_unref(packetPtr.get());
				}
			}
		}
		av_packet_unref(packetPtr.get());
		if (!is_av_interleaved_write_frame_success)
		{
			PV("av_interleaved_write_frame fails!");
		}
	}

	auto skipFrames = av_read_frame_cnt - av_interleaved_write_frame_cnt;
	PV("total:", av_read_frame_cnt, "; write: ", av_interleaved_write_frame_cnt, "; skip: ", skipFrames);

	// Flush decoder
	avcodec_send_packet(decoder_context, nullptr);
	while (avcodec_receive_frame(decoder_context, framePtr.get()) >= 0)
	{
		framePtr->pts = framePtr->best_effort_timestamp;
		last_decoded_pts = framePtr->pts;

		avcodec_send_frame(encoder_context, framePtr.get());
		while (avcodec_receive_packet(encoder_context, packetPtr.get()) >= 0)
		{
			av_interleaved_write_frame(output_format_context, packetPtr.get());
			av_packet_unref(packetPtr.get());
		}
	}

	// 将 new10.yuv .. new1.yuv 依次插入到视频结尾
	{
		if (last_decoded_pts == AV_NOPTS_VALUE)
		{
			last_decoded_pts = 0;
		}
		int64_t pts_counter = last_decoded_pts;

		const int w = encoder_context->width;
		const int h = encoder_context->height;
		const enum AVPixelFormat src_pix_fmt = AV_PIX_FMT_YUV420P;

		for (int n = 10; n >= 1; --n)
		{
			std::string fname = "new" + std::to_string(n) + ".yuv";
			FILE *in_fp{};
			if (fopen_s(&in_fp, fname.c_str(), "rb") != 0)
			{
				in_fp = nullptr;
			}
			if (!in_fp)
			{
				std::cout << "警告：无法打开插入帧文件：" << fname << " ，跳过该帧" << std::endl;
				continue;
			}

			AVFrame *ins_frame = av_frame_alloc();
			if (!ins_frame)
			{
				std::cout << "警告：av_frame_alloc 失败，跳过 " << fname << std::endl;
				if (in_fp) { fclose(in_fp); }
				continue;
			}
			AVFramePtr insFramePtr(ins_frame);   // ← 分配成功后接管

			insFramePtr->format = src_pix_fmt;
			insFramePtr->width = w;
			insFramePtr->height = h;
			if (av_frame_get_buffer(insFramePtr.get(), 32) < 0)
			{
				std::cout << "警告：av_frame_get_buffer 失败，跳过 " << fname << std::endl;
				// 修复 #2：不再手动 av_frame_free，由 insFramePtr 析构处理
				if (in_fp) { fclose(in_fp); }
				continue;
			}

			insFramePtr->format = src_pix_fmt;
			insFramePtr->width = w;
			insFramePtr->height = h;
			if (av_frame_get_buffer(insFramePtr.get(), 32) < 0)
			{
				std::cout << "警告：av_frame_get_buffer 失败，跳过 " << fname << std::endl;
				av_frame_free(&ins_frame);
				if (in_fp)
				{
					fclose(in_fp);
				}
				continue;
			}

			bool read_ok = true;
			for (int yy = 0; yy < h; ++yy)
			{
				size_t want = static_cast<size_t>(w);
				size_t got = fread(insFramePtr->data[0] + static_cast<size_t>(yy) * insFramePtr->linesize[0], 1, want, in_fp);
				if (got != want)
				{
					read_ok = false;
					break;
				}
			}
			for (int yy = 0; yy < h / 2 && read_ok; ++yy)
			{
				size_t want = static_cast<size_t>(w / 2);
				size_t got = fread(insFramePtr->data[1] + static_cast<size_t>(yy) * insFramePtr->linesize[1], 1, want, in_fp);
				if (got != want)
				{
					read_ok = false;
					break;
				}
			}
			for (int yy = 0; yy < h / 2 && read_ok; ++yy)
			{
				size_t want = static_cast<size_t>(w / 2);
				size_t got = fread(insFramePtr->data[2] + static_cast<size_t>(yy) * insFramePtr->linesize[2], 1, want, in_fp);
				if (got != want)
				{
					read_ok = false;
					break;
				}
			}
			if (in_fp)
			{
				fclose(in_fp);
			}

			if (!read_ok)
			{
				std::cout << "警告：从 " << fname << " 读取数据失败，跳过" << std::endl;
				continue;
			}

			pts_counter += 1;
			insFramePtr->pts = pts_counter;

			AVFrame *frame_to_send = insFramePtr.get();
			AVFrame *conv{};
			SwsContext *local_sws{};
			AVFramePtr convPtr(nullptr);
			if (encoder_context->pix_fmt != src_pix_fmt)
			{
				conv = av_frame_alloc();
				convPtr.reset(conv);
				if (!convPtr)
				{
					std::cout << "警告：无法分配 conv frame，直接发送原始帧（可能不被编码器接受）: " << fname << std::endl;
				}
				else
				{
					convPtr->format = encoder_context->pix_fmt;
					convPtr->width = w;
					convPtr->height = h;
					if (av_frame_get_buffer(convPtr.get(), 32) < 0)
					{
						std::cout << "警告：conv av_frame_get_buffer 失败，使用原始帧发送: " << fname << std::endl;
						convPtr.reset();
					}
					else
					{
						local_sws = sws_getContext(w, h, src_pix_fmt, w, h, encoder_context->pix_fmt, SWS_BILINEAR, nullptr, nullptr, nullptr);
						if (!local_sws)
						{
							std::cout << "警告：sws_getContext 转换失败，使用原始帧发送: " << fname << std::endl;
							convPtr.reset();
						}
						else
						{
							sws_scale(local_sws, insFramePtr->data, insFramePtr->linesize, 0, h, convPtr->data, convPtr->linesize);
							convPtr->pts = insFramePtr->pts;
							frame_to_send = convPtr.get();
						}
					}
				}
			}

			response = avcodec_send_frame(encoder_context, frame_to_send);
			if (response < 0)
			{
				std::cout << "Error while sending inserted frame " << fname << " to encoder: " << response << std::endl;
			}
			else
			{
				while ((response = avcodec_receive_packet(encoder_context, packetPtr.get())) >= 0)
				{
					av_packet_rescale_ts(packetPtr.get(), encoder_context->time_base, out_stream->time_base);
					packetPtr->stream_index = out_stream->index;
					av_interleaved_write_frame(output_format_context, packetPtr.get());
					av_packet_unref(packetPtr.get());
				}
			}

			if (local_sws)
			{
				sws_freeContext(local_sws);
			}
		}
	}

	// Flush encoder
	avcodec_send_frame(encoder_context, nullptr);
	while (avcodec_receive_packet(encoder_context, packetPtr.get()) >= 0)
	{
		av_interleaved_write_frame(output_format_context, packetPtr.get());
		av_packet_unref(packetPtr.get());
	}

	av_write_trailer(output_format_context);

	// 所有资源由 RAII unique_ptr 自动释放（inputFmtPtr/outputFmtPtr/decoderCtxPtr/encoderCtxPtr/framePtr/packetPtr 等）
	// 函数结束时 unique_ptr 的析构器会调用相应的释放函数
}

// DecodeAndEncodeAndReplaceMultiYuvs()实现：将输入MP4文件解码，替换指定区间的YUV帧为replaceYuvFiles中的YUV帧，并重新编码为输出MP4文件
// 1. 解码 input_filename，获取所有YUV帧
// 2. 遍历帧序列，将 [toReplaceYuvStartIdx, toReplaceYuvEndIdx] 区间的帧替换为 replaceYuvFiles 中的YUV帧
// 3. 重新编码所有帧为 output_filename
inline void DecodeAndEncodeAndReplaceMultiYuvs(const char *input_filename, const char *output_filename,
											   int toReplaceYuvStartIdx, int toReplaceYuvEndIdx, const std::vector<std::string> &replaceYuvFiles)
{
	if (!input_filename || !output_filename)
	{
		std::cout << "Invalid input or output filename" << std::endl;
		return;
	}
	if (toReplaceYuvStartIdx <= 0 || toReplaceYuvEndIdx < toReplaceYuvStartIdx)
	{
		std::cout << "Invalid replace index range: [" << toReplaceYuvStartIdx << ", " << toReplaceYuvEndIdx << "], no replacement will be performed" << std::endl;
	}
	// 修复 #4：校验 replaceYuvFiles 数量与替换区间长度一致
	const int expectedReplaceCount = toReplaceYuvEndIdx - toReplaceYuvStartIdx + 1;
	if (!replaceYuvFiles.empty() &&
		static_cast<int>(replaceYuvFiles.size()) != expectedReplaceCount)
	{
		std::cout << "Error: replaceYuvFiles.size() = " << replaceYuvFiles.size()
				<< " != range size " << expectedReplaceCount
				<< " ([start=" << toReplaceYuvStartIdx
				<< ", end=" << toReplaceYuvEndIdx << "])" << std::endl;
		return;   // ← 直接拒绝，避免静默出错
	}

	ON_SCOPE_EXIT
	{
		CountFrames(input_filename);
		CountFrames(output_filename);
	};

	AVFormatContext *raw_input_fmt = nullptr;
	if (avformat_open_input(&raw_input_fmt, input_filename, nullptr, nullptr) < 0)
	{
		std::cout << "Could not open input file: " << input_filename << std::endl;
		return;
	}
	AVFormatContextPtr input_fmt(raw_input_fmt);

	if (avformat_find_stream_info(input_fmt.get(), nullptr) < 0)
	{
		std::cout << "Failed to retrieve input stream information: " << input_filename << std::endl;
		return;
	}
	av_dump_format(input_fmt.get(), 0, input_filename, 0);

	int video_stream_index = -1;
	AVCodecParameters *codecpar = nullptr;
	const AVCodec *decoder = nullptr;
	for (unsigned int i = 0; i < input_fmt->nb_streams; ++i)
	{
		if (input_fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			video_stream_index = static_cast<int>(i);
			codecpar = input_fmt->streams[i]->codecpar;
			decoder = avcodec_find_decoder(codecpar->codec_id);
			break;
		}
	}
	if (video_stream_index == -1 || !decoder)
	{
		std::cout << "Could not find video stream in input: " << input_filename << std::endl;
		return;
	}

	AVCodecContext *raw_dec_ctx = avcodec_alloc_context3(decoder);
	if (!raw_dec_ctx)
	{
		std::cout << "Failed to allocate decoder context" << std::endl;
		return;
	}
	AVCodecContextPtr dec_ctx(raw_dec_ctx);

	if (avcodec_parameters_to_context(dec_ctx.get(), codecpar) < 0)
	{
		std::cout << "Failed to copy codec params to decoder context" << std::endl;
		return;
	}
	if (avcodec_open2(dec_ctx.get(), decoder, nullptr) < 0)
	{
		std::cout << "Failed to open decoder" << std::endl;
		return;
	}

	AVFormatContext *raw_out_fmt = nullptr;
	if (avformat_alloc_output_context2(&raw_out_fmt, nullptr, nullptr, output_filename) < 0 || !raw_out_fmt)
	{
		std::cout << "Could not create output context: " << output_filename << std::endl;
		return;
	}
	auto out_fmt_deleter = [](AVFormatContext *p)
	{
		if (p)
		{
			if (!(p->oformat->flags & AVFMT_NOFILE))
			{
				avio_closep(&p->pb);
			}
			avformat_free_context(p);
		}
	};
	std::unique_ptr<AVFormatContext, decltype(out_fmt_deleter)> out_fmt(raw_out_fmt, out_fmt_deleter);

	AVStream *out_stream = avformat_new_stream(out_fmt.get(), nullptr);
	if (!out_stream)
	{
		std::cout << "Failed allocating output stream" << std::endl;
		return;
	}

	// 开始
	// 替换原有 `const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);` 之后直到 avcodec_open2(...) 的代码段
	// 使用 libx264（如果可用）并传入质量控制参数（preset / crf / profile）
	const AVCodec *encoder = avcodec_find_encoder_by_name("libx264");
	if (!encoder)
	{
		// 回退到默认 h264 编码器
		encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
	}
	if (!encoder)
	{
		std::cout << "Encoder not found (H264/libx264)" << std::endl;
		return;
	}

	AVCodecContext *raw_enc_ctx = avcodec_alloc_context3(encoder);
	if (!raw_enc_ctx)
	{
		std::cout << "Failed to allocate encoder context" << std::endl;
		return;
	}
	AVCodecContextPtr enc_ctx(raw_enc_ctx);

	// 基本参数（强制 YUV420P 以兼容 libx264）
	auto [time_base, height, width, sample_aspect_ratio, bit_rate, pix_fmt, framerate] = GetAVStreamTempInfo(input_filename);
	enc_ctx->height = height;
	enc_ctx->width = width;
	enc_ctx->time_base = time_base;
	enc_ctx->framerate = framerate;
	// 使用较高比特率或依赖 CRF 控制质量；这里同时设置一个较高的 bitrate 作为保险
	enc_ctx->bit_rate = 4000000; // 4 Mbps
	enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	enc_ctx->gop_size = 10;
	enc_ctx->max_b_frames = 1;
	enc_ctx->codec_id = AV_CODEC_ID_H264;
	enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;

	if (out_fmt->oformat->flags & AVFMT_GLOBALHEADER)
	{
		enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	// 传入 libx264 特定选项：使用 CRF 控制质量，preset 控制速度/质量
	AVDictionary *enc_opts = nullptr;
	// 调整这些值：crf 18 ~ 23 为常用高质量范围；preset 可选 veryfast/medium/slow
	av_dict_set(&enc_opts, "preset", "slow", 0);
	av_dict_set(&enc_opts, "crf", "20", 0);		  // 质量参数，值越小质量越高（并增大文件）
	av_dict_set(&enc_opts, "profile", "high", 0); // high profile
	av_dict_set(&enc_opts, "tune", "film", 0);	  // 可选，根据内容调整
	av_dict_set(&enc_opts, "threads", "0", 0);	  // 自动线程

	if (avcodec_open2(enc_ctx.get(), encoder, &enc_opts) < 0)
	{
		std::cout << "Cannot open video encoder (libx264) with options" << std::endl;
		av_dict_free(&enc_opts);
		return;
	}
	av_dict_free(&enc_opts);

	// 之后仍需调用 avcodec_parameters_from_context(...)

	// 结束
	if (avcodec_parameters_from_context(out_stream->codecpar, enc_ctx.get()) < 0)
	{
		std::cout << "Failed to copy encoder parameters to output stream" << std::endl;
		return;
	}
	out_stream->time_base = enc_ctx->time_base;

	if (!(out_fmt->oformat->flags & AVFMT_NOFILE))
	{
		if (avio_open(&out_fmt->pb, output_filename, AVIO_FLAG_WRITE) < 0)
		{
			std::cout << "Could not open output file: " << output_filename << std::endl;
			return;
		}
	}

	if (avformat_write_header(out_fmt.get(), nullptr) < 0)
	{
		std::cout << "Error occurred when writing header to output" << std::endl;
		return;
	}

	AVPacket *raw_pkt = av_packet_alloc();
	AVPacketPtr pkt(raw_pkt);
	AVFrame *raw_frame = av_frame_alloc();
	AVFramePtr frame(raw_frame);
	if (!pkt || !frame)
	{
		std::cout << "Failed to allocate packet/frame" << std::endl;
		return;
	}

	int response = 0;
	int frame_index = 1;
	int64_t last_decoded_pts = AV_NOPTS_VALUE;
	bool inserted_replacements = false;

	while (av_read_frame(input_fmt.get(), pkt.get()) >= 0)
	{
		if (pkt->stream_index == video_stream_index)
		{
			response = avcodec_send_packet(dec_ctx.get(), pkt.get());
			if (response < 0)
			{
				std::cout << "Error sending packet to decoder: " << response << std::endl;
				av_packet_unref(pkt.get());
				break;
			}
			while (true)
			{
				response = avcodec_receive_frame(dec_ctx.get(), frame.get());
				if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
				{
					break;
				}
				if (response < 0)
				{
					std::cout << "Error receiving frame from decoder: " << response << std::endl;
					return;
				}

				frame->pts = frame->best_effort_timestamp;
				last_decoded_pts = frame->pts;

				if (frame_index == toReplaceYuvStartIdx && !replaceYuvFiles.empty() && !inserted_replacements)
				{
					for (const auto &replacePath : replaceYuvFiles)
					{
						FILE *raw_fp = nullptr;
#ifdef _WIN32
						if (fopen_s(&raw_fp, replacePath.c_str(), "rb") != 0)
						{
							raw_fp = nullptr;
						}
#else
						raw_fp = fopen(replacePath.c_str(), "rb");
#endif
						FilePtr rep_fp(raw_fp);
						if (!rep_fp)
						{
							std::cout << "Warning: cannot open replace YUV file: " << replacePath << std::endl;
							continue;
						}

						AVFrame *rep_frame = av_frame_alloc();
						AVFramePtr rep_frame_ptr(rep_frame);
						if (!rep_frame)
						{
							std::cout << "Warning: av_frame_alloc failed for replace frame: " << replacePath << std::endl;
							continue;
						}
						rep_frame->format = AV_PIX_FMT_YUV420P;
						rep_frame->width = static_cast<int>(enc_ctx->width);
						rep_frame->height = static_cast<int>(enc_ctx->height);
						if (av_frame_get_buffer(rep_frame, 32) < 0)
						{
							std::cout << "Warning: av_frame_get_buffer failed for: " << replacePath << std::endl;
							continue;
						}

						bool read_ok = true;
						for (int y = 0; y < rep_frame->height && read_ok; ++y)
						{
							size_t want = static_cast<size_t>(rep_frame->width);
							size_t got = fread(rep_frame->data[0] + static_cast<size_t>(y) * rep_frame->linesize[0], 1, want, rep_fp.get());
							if (got != want)
							{
								read_ok = false;
							}
						}
						for (int y = 0; y < rep_frame->height / 2 && read_ok; ++y)
						{
							size_t want = static_cast<size_t>(rep_frame->width / 2);
							size_t got = fread(rep_frame->data[1] + static_cast<size_t>(y) * rep_frame->linesize[1], 1, want, rep_fp.get());
							if (got != want)
							{
								read_ok = false;
							}
						}
						for (int y = 0; y < rep_frame->height / 2 && read_ok; ++y)
						{
							size_t want = static_cast<size_t>(rep_frame->width / 2);
							size_t got = fread(rep_frame->data[2] + static_cast<size_t>(y) * rep_frame->linesize[2], 1, want, rep_fp.get());
							if (got != want)
							{
								read_ok = false;
							}
						}
						if (!read_ok)
						{
							std::cout << "Warning: read YUV failed for: " << replacePath << std::endl;
							continue;
						}

						if (last_decoded_pts == AV_NOPTS_VALUE)
						{
							last_decoded_pts = 0;
						}
						last_decoded_pts += 1;
						rep_frame->pts = last_decoded_pts;

						AVFrame *frame_to_send = rep_frame;
						AVFrame *conv_frame = nullptr;
						SwsContext *conv_sws = nullptr;
						AVFramePtr conv_frame_ptr(nullptr);

						if (enc_ctx->pix_fmt != AV_PIX_FMT_YUV420P)
						{
							conv_frame = av_frame_alloc();
							conv_frame_ptr.reset(conv_frame);
							if (conv_frame)
							{
								conv_frame->format = enc_ctx->pix_fmt;
								conv_frame->width = rep_frame->width;
								conv_frame->height = rep_frame->height;
								if (av_frame_get_buffer(conv_frame, 32) == 0)
								{
									conv_sws = sws_getContext(rep_frame->width, rep_frame->height, AV_PIX_FMT_YUV420P,
															  rep_frame->width, rep_frame->height, enc_ctx->pix_fmt,
															  SWS_BILINEAR, nullptr, nullptr, nullptr);
									if (conv_sws)
									{
										sws_scale(conv_sws, rep_frame->data, rep_frame->linesize, 0, rep_frame->height,
												  conv_frame->data, conv_frame->linesize);
										conv_frame->pts = rep_frame->pts;
										frame_to_send = conv_frame;
									}
								}
							}
						}

						response = avcodec_send_frame(enc_ctx.get(), frame_to_send);
						if (response < 0)
						{
							std::cout << "Error sending replacement frame to encoder: " << response << std::endl;
						}
						else
						{
							AVPacket *raw_out_pkt = av_packet_alloc();
							AVPacketPtr out_pkt(raw_out_pkt);
							while (avcodec_receive_packet(enc_ctx.get(), out_pkt.get()) == 0)
							{
								av_packet_rescale_ts(out_pkt.get(), enc_ctx->time_base, out_stream->time_base);
								out_pkt->stream_index = out_stream->index;
								av_interleaved_write_frame(out_fmt.get(), out_pkt.get());
								av_packet_unref(out_pkt.get());
							}
						}

						if (conv_sws)
						{
							sws_freeContext(conv_sws);
						}
					} // end for replace files

					inserted_replacements = true;
				} // end if reached start

				if (frame_index >= toReplaceYuvStartIdx && frame_index <= toReplaceYuvEndIdx)
				{
					frame_index += 1;
					continue;
				}

				response = avcodec_send_frame(enc_ctx.get(), frame.get());
				if (response < 0)
				{
					std::cout << "Error sending decoded frame to encoder: " << response << std::endl;
					return;
				}
				AVPacket *raw_out_pkt = av_packet_alloc();
				AVPacketPtr out_pkt(raw_out_pkt);
				while (avcodec_receive_packet(enc_ctx.get(), out_pkt.get()) == 0)
				{
					av_packet_rescale_ts(out_pkt.get(), enc_ctx->time_base, out_stream->time_base);
					out_pkt->stream_index = out_stream->index;
					av_interleaved_write_frame(out_fmt.get(), out_pkt.get());
					av_packet_unref(out_pkt.get());
				}

				frame_index += 1;
			}
		}
		av_packet_unref(pkt.get());
	}

	avcodec_send_packet(dec_ctx.get(), nullptr);
	while (avcodec_receive_frame(dec_ctx.get(), frame.get()) == 0)
	{
		frame->pts = frame->best_effort_timestamp;
		last_decoded_pts = frame->pts;

		if (frame_index == toReplaceYuvStartIdx && !replaceYuvFiles.empty() && !inserted_replacements)
		{
			for (const auto &replacePath : replaceYuvFiles)
			{
				FILE *raw_fp = nullptr;
#ifdef _WIN32
				if (fopen_s(&raw_fp, replacePath.c_str(), "rb") != 0)
				{
					raw_fp = nullptr;
				}
#else
				raw_fp = fopen(replacePath.c_str(), "rb");
#endif
				FilePtr rep_fp(raw_fp);
				if (!rep_fp)
				{
					std::cout << "Warning: cannot open replace YUV file: " << replacePath << std::endl;
					continue;
				}

				AVFrame *rep_frame = av_frame_alloc();
				AVFramePtr rep_frame_ptr(rep_frame);
				if (!rep_frame)
				{
					std::cout << "Warning: av_frame_alloc failed for replace frame: " << replacePath << std::endl;
					continue;
				}
				rep_frame->format = AV_PIX_FMT_YUV420P;
				rep_frame->width = static_cast<int>(enc_ctx->width);
				rep_frame->height = static_cast<int>(enc_ctx->height);
				if (av_frame_get_buffer(rep_frame, 32) < 0)
				{
					std::cout << "Warning: av_frame_get_buffer failed for: " << replacePath << std::endl;
					continue;
				}

				bool read_ok = true;
				for (int y = 0; y < rep_frame->height && read_ok; ++y)
				{
					size_t want = static_cast<size_t>(rep_frame->width);
					size_t got = fread(rep_frame->data[0] + static_cast<size_t>(y) * rep_frame->linesize[0], 1, want, rep_fp.get());
					if (got != want)
					{
						read_ok = false;
					}
				}
				for (int y = 0; y < rep_frame->height / 2 && read_ok; ++y)
				{
					size_t want = static_cast<size_t>(rep_frame->width / 2);
					size_t got = fread(rep_frame->data[1] + static_cast<size_t>(y) * rep_frame->linesize[1], 1, want, rep_fp.get());
					if (got != want)
					{
						read_ok = false;
					}
				}
				for (int y = 0; y < rep_frame->height / 2 && read_ok; ++y)
				{
					size_t want = static_cast<size_t>(rep_frame->width / 2);
					size_t got = fread(rep_frame->data[2] + static_cast<size_t>(y) * rep_frame->linesize[2], 1, want, rep_fp.get());
					if (got != want)
					{
						read_ok = false;
					}
				}
				if (!read_ok)
				{
					std::cout << "Warning: read YUV failed for: " << replacePath << std::endl;
					continue;
				}

				if (last_decoded_pts == AV_NOPTS_VALUE)
				{
					last_decoded_pts = 0;
				}
				last_decoded_pts += 1;
				rep_frame->pts = last_decoded_pts;

				AVFrame *frame_to_send = rep_frame;
				AVFrame *conv_frame = nullptr;
				SwsContext *conv_sws = nullptr;
				AVFramePtr conv_frame_ptr(nullptr);

				if (enc_ctx->pix_fmt != AV_PIX_FMT_YUV420P)
				{
					conv_frame = av_frame_alloc();
					conv_frame_ptr.reset(conv_frame);
					if (conv_frame)
					{
						conv_frame->format = enc_ctx->pix_fmt;
						conv_frame->width = rep_frame->width;
						conv_frame->height = rep_frame->height;
						if (av_frame_get_buffer(conv_frame, 32) == 0)
						{
							conv_sws = sws_getContext(rep_frame->width, rep_frame->height, AV_PIX_FMT_YUV420P,
													  rep_frame->width, rep_frame->height, enc_ctx->pix_fmt,
													  SWS_BILINEAR, nullptr, nullptr, nullptr);
							if (conv_sws)
							{
								sws_scale(conv_sws, rep_frame->data, rep_frame->linesize, 0, rep_frame->height,
										  conv_frame->data, conv_frame->linesize);
								conv_frame->pts = rep_frame->pts;
								frame_to_send = conv_frame;
							}
						}
					}
				}

				response = avcodec_send_frame(enc_ctx.get(), frame_to_send);
				if (response < 0)
				{
					std::cout << "Error sending replacement tail frame to encoder: " << response << std::endl;
				}
				else
				{
					AVPacket *raw_out_pkt = av_packet_alloc();
					AVPacketPtr out_pkt(raw_out_pkt);
					while (avcodec_receive_packet(enc_ctx.get(), out_pkt.get()) == 0)
					{
						av_packet_rescale_ts(out_pkt.get(), enc_ctx->time_base, out_stream->time_base);
						out_pkt->stream_index = out_stream->index;
						av_interleaved_write_frame(out_fmt.get(), out_pkt.get());
						av_packet_unref(out_pkt.get());
					}
				}

				if (conv_sws)
				{
					sws_freeContext(conv_sws);
				}
			} // end insert replacements when tail

			inserted_replacements = true;
		}

		if (frame_index >= toReplaceYuvStartIdx && frame_index <= toReplaceYuvEndIdx)
		{
			frame_index += 1;
			continue;
		}

		response = avcodec_send_frame(enc_ctx.get(), frame.get());
		if (response < 0)
		{
			std::cout << "Error sending flushed decoded frame to encoder: " << response << std::endl;
			return;
		}
		AVPacket *raw_out_pkt = av_packet_alloc();
		AVPacketPtr out_pkt(raw_out_pkt);
		while (avcodec_receive_packet(enc_ctx.get(), out_pkt.get()) == 0)
		{
			av_packet_rescale_ts(out_pkt.get(), enc_ctx->time_base, out_stream->time_base);
			out_pkt->stream_index = out_stream->index;
			av_interleaved_write_frame(out_fmt.get(), out_pkt.get());
			av_packet_unref(out_pkt.get());
		}

		frame_index += 1;
	}

	if (!inserted_replacements && !replaceYuvFiles.empty() && frame_index <= toReplaceYuvStartIdx)
	{
		for (const auto &replacePath : replaceYuvFiles)
		{
			FILE *raw_fp = nullptr;
#ifdef _WIN32
			if (fopen_s(&raw_fp, replacePath.c_str(), "rb") != 0)
			{
				raw_fp = nullptr;
			}
#else
			raw_fp = fopen(replacePath.c_str(), "rb");
#endif
			FilePtr rep_fp(raw_fp);
			if (!rep_fp)
			{
				std::cout << "Warning: cannot open replace YUV file: " << replacePath << std::endl;
				continue;
			}

			AVFrame *rep_frame = av_frame_alloc();
			AVFramePtr rep_frame_ptr(rep_frame);
			if (!rep_frame)
			{
				std::cout << "Warning: av_frame_alloc failed for replace frame: " << replacePath << std::endl;
				continue;
			}
			rep_frame->format = AV_PIX_FMT_YUV420P;
			rep_frame->width = static_cast<int>(enc_ctx->width);
			rep_frame->height = static_cast<int>(enc_ctx->height);
			if (av_frame_get_buffer(rep_frame, 32) < 0)
			{
				std::cout << "Warning: av_frame_get_buffer failed for: " << replacePath << std::endl;
				continue;
			}

			bool read_ok = true;
			for (int y = 0; y < rep_frame->height && read_ok; ++y)
			{
				size_t want = static_cast<size_t>(rep_frame->width);
				size_t got = fread(rep_frame->data[0] + static_cast<size_t>(y) * rep_frame->linesize[0], 1, want, rep_fp.get());
				if (got != want)
				{
					read_ok = false;
				}
			}
			for (int y = 0; y < rep_frame->height / 2 && read_ok; ++y)
			{
				size_t want = static_cast<size_t>(rep_frame->width / 2);
				size_t got = fread(rep_frame->data[1] + static_cast<size_t>(y) * rep_frame->linesize[1], 1, want, rep_fp.get());
				if (got != want)
				{
					read_ok = false;
				}
			}
			for (int y = 0; y < rep_frame->height / 2 && read_ok; ++y)
			{
				size_t want = static_cast<size_t>(rep_frame->width / 2);
				size_t got = fread(rep_frame->data[2] + static_cast<size_t>(y) * rep_frame->linesize[2], 1, want, rep_fp.get());
				if (got != want)
				{
					read_ok = false;
				}
			}
			if (!read_ok)
			{
				std::cout << "Warning: read YUV failed for: " << replacePath << std::endl;
				continue;
			}

			if (last_decoded_pts == AV_NOPTS_VALUE)
			{
				last_decoded_pts = 0;
			}
			last_decoded_pts += 1;
			rep_frame->pts = last_decoded_pts;

			AVFrame *frame_to_send = rep_frame;
			AVFrame *conv_frame = nullptr;
			SwsContext *conv_sws = nullptr;
			AVFramePtr conv_frame_ptr(nullptr);

			if (enc_ctx->pix_fmt != AV_PIX_FMT_YUV420P)
			{
				conv_frame = av_frame_alloc();
				conv_frame_ptr.reset(conv_frame);
				if (conv_frame)
				{
					conv_frame->format = enc_ctx->pix_fmt;
					conv_frame->width = rep_frame->width;
					conv_frame->height = rep_frame->height;
					if (av_frame_get_buffer(conv_frame, 32) == 0)
					{
						conv_sws = sws_getContext(rep_frame->width, rep_frame->height, AV_PIX_FMT_YUV420P,
												  rep_frame->width, rep_frame->height, enc_ctx->pix_fmt,
												  SWS_BILINEAR, nullptr, nullptr, nullptr);
						if (conv_sws)
						{
							sws_scale(conv_sws, rep_frame->data, rep_frame->linesize, 0, rep_frame->height,
									  conv_frame->data, conv_frame->linesize);
							conv_frame->pts = rep_frame->pts;
							frame_to_send = conv_frame;
						}
					}
				}
			}

			response = avcodec_send_frame(enc_ctx.get(), frame_to_send);
			if (response < 0)
			{
				std::cout << "Error sending appended replacement frame to encoder: " << response << std::endl;
			}
			else
			{
				AVPacket *raw_out_pkt = av_packet_alloc();
				AVPacketPtr out_pkt(raw_out_pkt);
				while (avcodec_receive_packet(enc_ctx.get(), out_pkt.get()) == 0)
				{
					av_packet_rescale_ts(out_pkt.get(), enc_ctx->time_base, out_stream->time_base);
					out_pkt->stream_index = out_stream->index;
					av_interleaved_write_frame(out_fmt.get(), out_pkt.get());
					av_packet_unref(out_pkt.get());
				}
			}

			if (conv_sws)
			{
				sws_freeContext(conv_sws);
			}
		}
	}

	avcodec_send_frame(enc_ctx.get(), nullptr);
	while (true)
	{
		int ret = avcodec_receive_packet(enc_ctx.get(), pkt.get());
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			break;
		}
		if (ret < 0)
		{
			std::cout << "Error while receiving packet from encoder during flush: " << ret << std::endl;
			break;
		}
		av_packet_rescale_ts(pkt.get(), enc_ctx->time_base, out_stream->time_base);
		pkt->stream_index = out_stream->index;
		av_interleaved_write_frame(out_fmt.get(), pkt.get());
		av_packet_unref(pkt.get());
	}

	av_write_trailer(out_fmt.get());

	std::cout << "DecodeAndEncodeAndReplaceMultiYuvs finished: " << output_filename << std::endl;
}

// 将in_filename 做解码，解码的每帧yuv存入 outDir路径下的 1.yuv 2.yuv ...
inline int DecodeMp4ToYuvFrames(const char *in_filename, std::string outDir = "")
{
	if (!in_filename)
	{
		std::cout << "Invalid input filename" << std::endl;
		return -1;
	}

	GetAVStreamTempInfo(in_filename);

	AVFormatContext *raw_fmt_ctx{};
	if (avformat_open_input(&raw_fmt_ctx, in_filename, nullptr, nullptr) < 0)
	{
		std::cout << "无法打开输入文件: " << in_filename << std::endl;
		return -1;
	}
	AVFormatContextPtr fmt_ctx(raw_fmt_ctx);

	if (avformat_find_stream_info(fmt_ctx.get(), nullptr) < 0)
	{
		std::cout << "无法读取流信息" << std::endl;
		return -1;
	}

	// 查找视频流
	int video_stream_index{-1};
	for (unsigned i = 0; i < fmt_ctx->nb_streams; ++i)
	{
		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			video_stream_index = static_cast<int>(i);
			break;
		}
	}
	if (video_stream_index < 0)
	{
		std::cout << "未找到视频流" << std::endl;
		return -1;
	}

	// 获取 codecpar 并查找解码器
	AVCodecParameters *codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
	if (!codecpar)
	{
		std::cout << "流参数为空" << std::endl;
		return -1;
	}
	const AVCodec *decoder = avcodec_find_decoder(codecpar->codec_id);
	if (!decoder)
	{
		std::cout << "找不到解码器" << std::endl;
		return -1;
	}

	// 解码器上下文（RAII）
	AVCodecContext *raw_dec_ctx = avcodec_alloc_context3(decoder);
	if (!raw_dec_ctx)
	{
		std::cout << "无法分配解码器上下文" << std::endl;
		return -1;
	}
	AVCodecContextPtr dec_ctx(raw_dec_ctx);

	if (avcodec_parameters_to_context(dec_ctx.get(), codecpar) < 0)
	{
		std::cout << "从参数创建解码器上下文失败" << std::endl;
		return -1;
	}
	if (avcodec_open2(dec_ctx.get(), decoder, nullptr) < 0)
	{
		std::cout << "打开解码器失败" << std::endl;
		return -1;
	}

	// out_dir 由参数决定：若 outDir 为空字符串则使用当前代码路径，否则使用传入路径
	fs::path out_dir;
	if (outDir.empty())
	{
		out_dir = fs::current_path();
	}
	else
	{
		out_dir = fs::path(outDir);
		if (out_dir.has_extension() && out_dir.extension() == ".yuv")
		{
			out_dir = out_dir.parent_path();
		}
		if (out_dir.empty())
		{
			out_dir = fs::current_path();
		}
	}

	std::error_code ec;
	if (!fs::exists(out_dir) && !fs::create_directories(out_dir, ec))
	{
		if (ec)
		{
			std::cout << "无法创建输出目录: " << out_dir.string() << ", 错误: " << ec.message() << std::endl;
			return -1;
		}
	}

	// 分配 AVPacket/AVFrame 并使用 RAII 包装
	AVPacket *raw_pkt = av_packet_alloc();
	AVPacketPtr pkt(raw_pkt);
	AVFrame *raw_frame = av_frame_alloc();
	AVFramePtr frame(raw_frame);
	AVFrame *raw_yuv = av_frame_alloc();
	AVFramePtr yuv(raw_yuv);

	if (!pkt || !frame || !yuv)
	{
		std::cout << "内存分配失败" << std::endl;
		return -1;
	}

	// 从 codecpar 获取宽高（避免 magic 0 值）
	int width = codecpar->width;
	int height = codecpar->height;
	if (width <= 0 || height <= 0)
	{
		std::cout << "无效的宽高： " << width << " x " << height << std::endl;
		return -1;
	}

	Dump(YUV_WH_FILE, width, height);

	// 为目标 YUV420P 帧分配缓冲
	yuv->format = AV_PIX_FMT_YUV420P;
	yuv->width = width;
	yuv->height = height;
	if (av_frame_get_buffer(yuv.get(), 32) < 0)
	{
		std::cout << "无法分配 yuv 帧缓冲" << std::endl;
		return -1;
	}

	// sws 上下文：若输入格式不是 YUV420P 则用于转换（RAII）
	SwsContext *raw_sws_ctx{};
	SwsCtxPtr sws_ctx(nullptr);

	// Helper: 将单帧写入单独文件，文件名为 {index}.yuv
	auto write_frame_to_file = [&](AVFrame *out_frame, int idx) -> int
	{
		fs::path fname = out_dir / (std::to_string(idx) + ".yuv");
		FILE *raw_fp2{};
#ifdef _WIN32
		if (fopen_s(&raw_fp2, fname.string().c_str(), "wb") != 0)
		{
			raw_fp2 = nullptr;
		}
#else
		raw_fp2 = fopen(fname.string().c_str(), "wb");
#endif
		FilePtr fp2(raw_fp2);
		if (!fp2)
		{
			std::cout << "无法打开输出文件: " << fname.string() << std::endl;
			return -1;
		}

		// Y plane
		for (int y = 0; y < height; ++y)
		{
			if (fwrite(out_frame->data[0] + (size_t)y * out_frame->linesize[0], 1, width, fp2.get()) != (size_t)width)
			{
				std::cout << "写入 Y 平面失败: " << fname.string() << std::endl;
				return -1;
			}
		}
		// U plane
		for (int y = 0; y < height / 2; ++y)
		{
			if (fwrite(out_frame->data[1] + (size_t)y * out_frame->linesize[1], 1, width / 2, fp2.get()) != (size_t)(width / 2))
			{
				std::cout << "写入 U 平面失败: " << fname.string() << std::endl;
				return -1;
			}
		}
		// V plane
		for (int y = 0; y < height / 2; ++y)
		{
			if (fwrite(out_frame->data[2] + (size_t)y * out_frame->linesize[2], 1, width / 2, fp2.get()) != (size_t)(width / 2))
			{
				std::cout << "写入 V 平面失败: " << fname.string() << std::endl;
				return -1;
			}
		}
		return 0;
	};

	// 可选：MJPEG 用于生成 JPG
	const AVCodec *jpeg_encoder = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
	AVCodecContext *jpeg_enc_ctx{};
	AVPacket *raw_jpeg_pkt = av_packet_alloc();
	AVPacketPtr jpeg_pkt{raw_jpeg_pkt};
	AVFrame *jpeg_conv_frame{};
	SwsContext *jpeg_sws_ctx_raw{};
	SwsCtxPtr jpeg_sws_ctx{};

	if (jpeg_encoder)
	{
		jpeg_enc_ctx = avcodec_alloc_context3(jpeg_encoder);
		if (jpeg_enc_ctx)
		{
			// 使用非弃用的 YUV420P，且显式声明 JPEG 的 full range
			jpeg_enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P; // 关键点：不要使用已弃用的 AV_PIX_FMT_YUVJ420P 作为 sws 的目标像素格式，
			jpeg_enc_ctx->height = height;
			jpeg_enc_ctx->width = width;
			jpeg_enc_ctx->time_base = AVRational{1, 25};
			jpeg_enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
			jpeg_enc_ctx->color_range = AVCOL_RANGE_JPEG;

			// 优化质量参数：使用常见键 "quality" 或 "q"（依赖编译时后端）
			AVDictionary *jpeg_opts{};
			// 1..100 quality（libjpeg 风格） —— 优先尝试 quality
			av_dict_set(&jpeg_opts, "quality", "100", 0); // 生成220KB的单张.jpg文件（之前是20KB）
			// 作为后备也设置 qscale/q（某些后端识别）
			av_dict_set(&jpeg_opts, "q", "2", 0);
			av_dict_set(&jpeg_opts, "qmin", "2", 0);
			av_dict_set(&jpeg_opts, "qmax", "2", 0);

			if (avcodec_open2(jpeg_enc_ctx, jpeg_encoder, &jpeg_opts) < 0)
			{
				std::cout << "无法打开 MJPEG 编码器（JPEG 生成功能将被禁用）" << std::endl;
				avcodec_free_context(&jpeg_enc_ctx);
				jpeg_enc_ctx = nullptr;
			}
			av_dict_free(&jpeg_opts);
		}
	}

	int frame_index = 1;

	// 读取并解码每个包
	while (av_read_frame(fmt_ctx.get(), pkt.get()) >= 0)
	{
		if (pkt->stream_index == video_stream_index)
		{
			if (avcodec_send_packet(dec_ctx.get(), pkt.get()) < 0)
			{
				av_packet_unref(pkt.get());
				continue;
			}
			while (avcodec_receive_frame(dec_ctx.get(), frame.get()) == 0)
			{
				AVFrame *out_frame{};
				if (dec_ctx->pix_fmt != AV_PIX_FMT_YUV420P)
				{
					if (!sws_ctx)
					{
						raw_sws_ctx = sws_getContext(width, height, dec_ctx->pix_fmt,
													 width, height, AV_PIX_FMT_YUV420P,
													 SWS_BILINEAR, nullptr, nullptr, nullptr);
						if (!raw_sws_ctx)
						{
							std::cout << "无法创建 sws 上下文" << std::endl;
							return -1;
						}
						sws_ctx.reset(raw_sws_ctx);
					}
					sws_scale(sws_ctx.get(),
							  frame->data, frame->linesize,
							  0, height,
							  yuv->data, yuv->linesize);
					out_frame = yuv.get();
				}
				else
				{
					out_frame = frame.get();
				}

				if (write_frame_to_file(out_frame, frame_index) < 0)
				{
					return -1;
				}

				// 生成 JPEG（可选）
				if (jpeg_enc_ctx && jpeg_pkt)
				{
					AVFrame *frame_for_jpeg{};
					if (out_frame->format != jpeg_enc_ctx->pix_fmt)
					{
						if (!jpeg_conv_frame)
						{
							jpeg_conv_frame = av_frame_alloc();
							if (jpeg_conv_frame)
							{
								jpeg_conv_frame->format = jpeg_enc_ctx->pix_fmt;
								jpeg_conv_frame->width = width;
								jpeg_conv_frame->height = height;
								jpeg_conv_frame->color_range = jpeg_enc_ctx->color_range;
								if (av_frame_get_buffer(jpeg_conv_frame, 32) < 0)
								{
									av_frame_free(&jpeg_conv_frame);
									jpeg_conv_frame = nullptr;
								}
							}
						}
						if (jpeg_conv_frame)
						{
							if (!jpeg_sws_ctx)
							{
								jpeg_sws_ctx_raw = sws_getContext(width, height, static_cast<AVPixelFormat>(out_frame->format),
																  width, height, jpeg_enc_ctx->pix_fmt,
																  SWS_BILINEAR, nullptr, nullptr, nullptr);
								if (!jpeg_sws_ctx_raw)
								{
									std::cout << "无法创建 jpeg sws 上下文" << std::endl;
									// 继续，不影响 YUV 写出
								}
								else
								{
									jpeg_sws_ctx.reset(jpeg_sws_ctx_raw);
								}
							}
							if (jpeg_sws_ctx)
							{
								sws_scale(jpeg_sws_ctx.get(),
										  out_frame->data, out_frame->linesize,
										  0, height,
										  jpeg_conv_frame->data, jpeg_conv_frame->linesize);
								jpeg_conv_frame->color_range = jpeg_enc_ctx->color_range;
								frame_for_jpeg = jpeg_conv_frame;
							}
						}
					}
					else
					{
						frame_for_jpeg = out_frame;
					}

					if (frame_for_jpeg)
					{
						frame_for_jpeg->pts = frame_index;
						if (avcodec_send_frame(jpeg_enc_ctx, frame_for_jpeg) == 0)
						{
							while (avcodec_receive_packet(jpeg_enc_ctx, jpeg_pkt.get()) == 0)
							{
								fs::path jpg_path = out_dir / (std::to_string(frame_index) + ".jpg");
								FILE *jpg_fp{};
#ifdef _WIN32
								if (fopen_s(&jpg_fp, jpg_path.string().c_str(), "wb") == 0 && jpg_fp)
								{
#else
								jpg_fp = fopen(jpg_path.string().c_str(), "wb");
								if (jpg_fp)
								{
#endif
									fwrite(jpeg_pkt->data, 1, jpeg_pkt->size, jpg_fp);
#ifdef _WIN32
									fclose(jpg_fp);
#else
									fclose(jpg_fp);
#endif
								}
								av_packet_unref(jpeg_pkt.get());
								break;
							}
						}
					}
				}

				++frame_index;
			}
		}
		av_packet_unref(pkt.get());
	}

	// 发送 NULL 刷新解码器缓冲
	avcodec_send_packet(dec_ctx.get(), nullptr);
	while (avcodec_receive_frame(dec_ctx.get(), frame.get()) == 0)
	{
		AVFrame *out_frame{};
		if (dec_ctx->pix_fmt != AV_PIX_FMT_YUV420P)
		{
			if (!sws_ctx)
			{
				raw_sws_ctx = sws_getContext(width, height, dec_ctx->pix_fmt,
											 width, height, AV_PIX_FMT_YUV420P,
											 SWS_BILINEAR, nullptr, nullptr, nullptr);
				if (!raw_sws_ctx)
				{
					std::cout << "无法创建 sws 上下文" << std::endl;
					return -1;
				}
				sws_ctx.reset(raw_sws_ctx);
			}
			sws_scale(sws_ctx.get(), frame->data, frame->linesize, 0, height, yuv->data, yuv->linesize);
			out_frame = yuv.get();
		}
		else
		{
			out_frame = frame.get();
		}

		if (write_frame_to_file(out_frame, frame_index) < 0)
		{
			return -1;
		}

		// 也为刷新阶段的帧生成 JPEG（与上文相同的逻辑）
		if (jpeg_enc_ctx && jpeg_pkt)
		{
			AVFrame *frame_for_jpeg{};
			if (out_frame->format != jpeg_enc_ctx->pix_fmt)
			{
				if (jpeg_conv_frame && jpeg_sws_ctx)
				{
					sws_scale(jpeg_sws_ctx.get(),
							  out_frame->data, out_frame->linesize,
							  0, height,
							  jpeg_conv_frame->data, jpeg_conv_frame->linesize);
					frame_for_jpeg = jpeg_conv_frame;
				}
			}
			else
			{
				frame_for_jpeg = out_frame;
			}

			if (frame_for_jpeg)
			{
				frame_for_jpeg->pts = 0;
				if (avcodec_send_frame(jpeg_enc_ctx, frame_for_jpeg) == 0)
				{
					while (avcodec_receive_packet(jpeg_enc_ctx, jpeg_pkt.get()) == 0)
					{
						fs::path jpg_path = out_dir / (std::to_string(frame_index) + ".jpg");
						FILE *jpg_fp{};
#ifdef _WIN32
						if (fopen_s(&jpg_fp, jpg_path.string().c_str(), "wb") == 0 && jpg_fp)
						{
#else
						jpg_fp = fopen(jpg_path.string().c_str(), "wb");
						if (jpg_fp)
						{
#endif
							fwrite(jpeg_pkt->data, 1, jpeg_pkt->size, jpg_fp);
#ifdef _WIN32
							fclose(jpg_fp);
#else
							fclose(jpg_fp);
#endif
						}
						av_packet_unref(jpeg_pkt.get());
						break;
					}
				}
			}
		}

		++frame_index;
	}

	// flush jpeg encoder
	if (jpeg_enc_ctx && jpeg_pkt)
	{
		avcodec_send_frame(jpeg_enc_ctx, nullptr);
		while (avcodec_receive_packet(jpeg_enc_ctx, jpeg_pkt.get()) == 0)
		{
			fs::path jpg_path = out_dir / (std::to_string(frame_index) + ".jpg");
			FILE *jpg_fp{};
#ifdef _WIN32
			if (fopen_s(&jpg_fp, jpg_path.string().c_str(), "wb") == 0 && jpg_fp)
			{
#else
			jpg_fp = fopen(jpg_path.string().c_str(), "wb");
			if (jpg_fp)
			{
#endif
				fwrite(jpeg_pkt->data, 1, jpeg_pkt->size, jpg_fp);
#ifdef _WIN32
				fclose(jpg_fp);
#else
				fclose(jpg_fp);
#endif
			}
			av_packet_unref(jpeg_pkt.get());
			++frame_index;
		}
	}

	// 释放 JPEG 相关资源（若使用）
	if (jpeg_sws_ctx)
	{
		sws_freeContext(jpeg_sws_ctx.release());
	}
	if (jpeg_conv_frame)
	{
		av_frame_free(&jpeg_conv_frame);
	}
	if (jpeg_pkt)
	{
		// jpeg_pkt 为 AVPacketPtr，会在析构时释放
	}
	if (jpeg_enc_ctx)
	{
		avcodec_free_context(&jpeg_enc_ctx);
	}

	return 0;
}

inline void DecodeEncode(const char* input_filename, const char* output_filename)
{
	ON_SCOPE_EXIT
	{
		CountFrames(input_filename);
		CountFrames(output_filename);
	};
	AVFormatContext* input_format_context{};
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

	const AVCodec* decoder{};
	AVCodecParameters* codec_params{};
	int video_stream_index{ -1 };
	for (unsigned int i = 0; i < input_format_context->nb_streams; i++)
	{
		AVCodecParameters* local_codec_params = input_format_context->streams[i]->codecpar;
		const AVCodec* local_codec = avcodec_find_decoder(local_codec_params->codec_id);

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

	AVCodecContext* decoder_context{}; // 值初始化
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

	AVFormatContext* output_format_context{};
	avformat_alloc_output_context2(&output_format_context, nullptr, nullptr, output_filename);
	if (!output_format_context)
	{
		std::cout << "Could not create output context" << std::endl;
		return;
	}

	AVStream* out_stream{};
	out_stream = avformat_new_stream(output_format_context, nullptr);
	if (!out_stream)
	{
		std::cout << "Failed allocating output stream" << std::endl;
		return;
	}

	const AVCodec* encoder{};
	encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!encoder)
	{
		std::cout << "Necessary encoder not found" << std::endl;
		return;
	}

	AVCodecContext* encoder_context{};
	encoder_context = avcodec_alloc_context3(encoder);
	if (!encoder_context)
	{
		std::cout << "Failed to allocate the encoder context" << std::endl;
		return;
	}

	if (avcodec_parameters_to_context(encoder_context, codec_params) < 0)
	{
		std::cout << "Failed to copy codec params to encoder_context " << std::endl;
		return;
	}

	auto [time_base, height, width, sample_aspect_ratio, bit_rate, pix_fmt, framerate] = GetAVStreamTempInfo(input_filename);

	encoder_context->time_base = time_base;
	encoder_context->height = height;
	encoder_context->width = width;
	encoder_context->sample_aspect_ratio = sample_aspect_ratio;
	encoder_context->bit_rate = 400000;
	encoder_context->pix_fmt = pix_fmt;
	encoder_context->framerate = framerate;

	encoder_context->codec_id = AV_CODEC_ID_H264;
	encoder_context->codec_type = AVMEDIA_TYPE_VIDEO;

	encoder_context->gop_size = 10;
	encoder_context->max_b_frames = 1;

	if (output_format_context->oformat->flags & AVFMT_GLOBALHEADER)
	{
		encoder_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	if (avcodec_open2(encoder_context, encoder, nullptr) < 0)
	{
		std::cout << "Cannot open video encoder for stream" << std::endl;
		return;
	}

	if (avcodec_parameters_from_context(out_stream->codecpar, encoder_context) < 0)
	{
		std::cout << "Failed to copy encoder parameters to output stream" << std::endl;
		return;
	}

	out_stream->time_base = encoder_context->time_base;

	if (!(output_format_context->oformat->flags & AVFMT_NOFILE))
	{
		if (avio_open(&output_format_context->pb, output_filename, AVIO_FLAG_WRITE) < 0)
		{
			std::cout << "Could not open output file 『" << output_filename << "』" << std::endl;
			return;
		}
	}

	if (avformat_write_header(output_format_context, nullptr) < 0)
	{
		std::cout << "Error occurred when opening output file" << std::endl;
		return;
	}

	AVFrame* frame{};
	AVPacket* packet{};
	frame = av_frame_alloc();
	packet = av_packet_alloc();
	int response{};

	int av_read_frame_cnt{ 1 };
	int av_interleaved_write_frame_cnt{ 1 };

	int64_t last_decoded_pts = AV_NOPTS_VALUE;

	while (av_read_frame(input_format_context, packet) >= 0)
	{
		bool is_av_interleaved_write_frame_success{};
		PV("av_read_frame_cnt:", av_read_frame_cnt++);
		if (packet->stream_index == video_stream_index)
		{
			response = avcodec_send_packet(decoder_context, packet);
			if (response < 0)
			{
				std::cout << "Error while sending a packet to the decoder" << std::endl;
				break;
			}

			int out_response_pos_cnt{};
			while (response >= 0)
			{
				response = avcodec_receive_frame(decoder_context, frame);
				if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
				{
					PV("   out response==", response);
					break;
				}
				else if (response < 0)
				{
					std::cout << "Error while receiving a frame from the decoder" << std::endl;
					return;
				}

				frame->pts = frame->best_effort_timestamp;
				last_decoded_pts = frame->pts;

				frame->pict_type = AV_PICTURE_TYPE_NONE;

				response = avcodec_send_frame(encoder_context, frame);
				if (response < 0)
				{
					std::cout << "Error while sending a frame to the encoder" << std::endl;
					return;
				}

				int in_response_pos_cnt{};
				while (response >= 0)
				{
					PV("          in_response_pos_cnt:", in_response_pos_cnt++);
					response = avcodec_receive_packet(encoder_context, packet);
					if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
					{
						PV("        inner response==", response);
						break;
					}
					else if (response < 0)
					{
						std::cout << "Error while receiving a packet from the encoder" << std::endl;
						return;
					}
					av_packet_rescale_ts(packet, encoder_context->time_base, out_stream->time_base);
					packet->stream_index = out_stream->index;
					response = av_interleaved_write_frame(output_format_context, packet);
					is_av_interleaved_write_frame_success = true;
					av_interleaved_write_frame_cnt++;
					if (response < 0)
					{
						std::cout << "Error while writing packet to output file" << std::endl;
						return;
					}
					av_packet_unref(packet);
				}
			}
		}
		av_packet_unref(packet);
		if (!is_av_interleaved_write_frame_success)
		{
			PV("av_interleaved_write_frame fails!");
		}
	}

	auto skipFrames = av_read_frame_cnt - av_interleaved_write_frame_cnt;
	PV("total:", av_read_frame_cnt, "; write: ", av_interleaved_write_frame_cnt, "; skip: ", skipFrames);

	// Flush decoder
	avcodec_send_packet(decoder_context, nullptr);
	while (avcodec_receive_frame(decoder_context, frame) >= 0)
	{
		frame->pts = frame->best_effort_timestamp;
		last_decoded_pts = frame->pts;

		avcodec_send_frame(encoder_context, frame);
		while (avcodec_receive_packet(encoder_context, packet) >= 0)
		{
			av_interleaved_write_frame(output_format_context, packet);
			av_packet_unref(packet);
		}
	}

	// 不再插入额外合成帧（已移除插入单帧 YUV 的逻辑）

	// Flush encoder
	avcodec_send_frame(encoder_context, nullptr);
	while (avcodec_receive_packet(encoder_context, packet) >= 0)
	{
		av_interleaved_write_frame(output_format_context, packet);
		av_packet_unref(packet);
	}

	av_write_trailer(output_format_context);

	avcodec_free_context(&decoder_context);
	avcodec_free_context(&encoder_context);
	avformat_close_input(&input_format_context);
	if (output_format_context && !(output_format_context->oformat->flags & AVFMT_NOFILE))
	{
		avio_closep(&output_format_context->pb);
	}
	avformat_free_context(output_format_context);
	av_frame_free(&frame);
	av_packet_free(&packet);
}

inline void DecodeCropEncode(const char* input_filename, const char* output_filename, double xCropRatio, double yCropRatio)
{
	if (!input_filename || !output_filename)
	{
		std::cout << "Invalid input or output filename" << std::endl;
		return;
	}
	if (xCropRatio < 0.0 || xCropRatio >= 1.0 || yCropRatio < 0.0 || yCropRatio >= 1.0)
	{
		std::cout << "Invalid crop ratios. Must be in [0,1)." << std::endl;
		return;
	}

	ON_SCOPE_EXIT
	{
		CountFrames(input_filename);
		CountFrames(output_filename);
	};

	// 打开输入格式上下文（RAII）
	AVFormatContext* raw_in_fmt = nullptr;
	if (avformat_open_input(&raw_in_fmt, input_filename, nullptr, nullptr) < 0)
	{
		std::cout << "Could not open input file 『" << input_filename << "』" << std::endl;
		return;
	}
	AVFormatContextPtr inFmtPtr(raw_in_fmt);

	if (avformat_find_stream_info(inFmtPtr.get(), nullptr) < 0)
	{
		std::cout << "Failed to retrieve input stream information" << std::endl;
		return;
	}

	av_dump_format(inFmtPtr.get(), 0, input_filename, false);

	// 查找视频流和解码器
	int video_stream_index = -1;
	AVCodecParameters* codec_params = nullptr;
	const AVCodec* decoder = nullptr;
	for (unsigned int i = 0; i < inFmtPtr->nb_streams; ++i)
	{
		AVCodecParameters* p = inFmtPtr->streams[i]->codecpar;
		if (p->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			decoder = avcodec_find_decoder(p->codec_id);
			if (decoder)
			{
				video_stream_index = static_cast<int>(i);
				codec_params = p;
				break;
			}
		}
	}
	if (video_stream_index == -1 || !codec_params || !decoder)
	{
		std::cout << "Could not find video stream in the input, aborting" << std::endl;
		return;
	}

	// 解码器上下文（RAII）
	AVCodecContext* raw_dec_ctx = avcodec_alloc_context3(decoder);
	if (!raw_dec_ctx)
	{
		std::cout << "Failed to allocate memory for AVCodecContext" << std::endl;
		return;
	}
	AVCodecContextPtr decCtxPtr(raw_dec_ctx);
	if (avcodec_parameters_to_context(decCtxPtr.get(), codec_params) < 0)
	{
		std::cout << "Failed to copy codec params to codec context" << std::endl;
		return;
	}
	if (avcodec_open2(decCtxPtr.get(), decoder, nullptr) < 0)
	{
		std::cout << "Failed to open decoder" << std::endl;
		return;
	}

	// 获取流信息用于裁剪计算
	auto [time_base, orig_h, orig_w, sample_aspect_ratio, bit_rate, orig_pix_fmt, framerate] = GetAVStreamTempInfo(input_filename);
	if (orig_w <= 0 || orig_h <= 0)
	{
		std::cout << "Invalid source resolution" << std::endl;
		return;
	}

	// 计算裁剪区域（中心不变）
	int crop_left = static_cast<int>(std::floor(orig_w * (xCropRatio / 2.0)));
	int crop_top = static_cast<int>(std::floor(orig_h * (yCropRatio / 2.0)));
	int new_w = orig_w - 2 * crop_left;
	int new_h = orig_h - 2 * crop_top;
	if (new_w <= 0 || new_h <= 0)
	{
		std::cout << "Crop size invalid after applying ratios" << std::endl;
		return;
	}

	// 创建输出格式上下文（自定义 deleter 以正确关闭 pb 并释放 context）
	AVFormatContext* raw_out_fmt = nullptr;
	if (avformat_alloc_output_context2(&raw_out_fmt, nullptr, nullptr, output_filename) < 0 || !raw_out_fmt)
	{
		std::cout << "Could not create output context" << std::endl;
		return;
	}
	auto out_fmt_deleter = [](AVFormatContext* p)
		{
			if (p)
			{
				if (!(p->oformat->flags & AVFMT_NOFILE))
				{
					avio_closep(&p->pb);
				}
				avformat_free_context(p);
			}
		};
	std::unique_ptr<AVFormatContext, decltype(out_fmt_deleter)> outFmtPtr(raw_out_fmt, out_fmt_deleter);

	AVStream* out_stream = avformat_new_stream(outFmtPtr.get(), nullptr);
	if (!out_stream)
	{
		std::cout << "Failed allocating output stream" << std::endl;
		return;
	}

	// 选择编码器并创建编码器上下文（RAII）
	const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!encoder)
	{
		std::cout << "Necessary encoder not found" << std::endl;
		return;
	}
	AVCodecContext* raw_enc_ctx = avcodec_alloc_context3(encoder);
	if (!raw_enc_ctx)
	{
		std::cout << "Failed to allocate the encoder context" << std::endl;
		return;
	}
	AVCodecContextPtr encCtxPtr(raw_enc_ctx);

	// 复制输入参数到 encoder_context 然后覆盖宽高/像素格式/比特率等
	if (avcodec_parameters_to_context(encCtxPtr.get(), codec_params) < 0)
	{
		std::cout << "Failed to copy codec params to encoder_context " << std::endl;
		return;
	}
	encCtxPtr->width = new_w;
	encCtxPtr->height = new_h;
	encCtxPtr->time_base = time_base;
	encCtxPtr->framerate = framerate;
	encCtxPtr->sample_aspect_ratio = sample_aspect_ratio;
	encCtxPtr->bit_rate = bit_rate > 0 ? bit_rate : 400000;
	encCtxPtr->pix_fmt = orig_pix_fmt; // 保持原始格式，若编码器不支持则会转换
	encCtxPtr->codec_id = AV_CODEC_ID_H264;
	encCtxPtr->codec_type = AVMEDIA_TYPE_VIDEO;
	encCtxPtr->gop_size = 10;
	encCtxPtr->max_b_frames = 1;

	if (outFmtPtr->oformat->flags & AVFMT_GLOBALHEADER)
	{
		encCtxPtr->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	if (avcodec_open2(encCtxPtr.get(), encoder, nullptr) < 0)
	{
		std::cout << "Cannot open video encoder for stream" << std::endl;
		return;
	}

	if (avcodec_parameters_from_context(out_stream->codecpar, encCtxPtr.get()) < 0)
	{
		std::cout << "Failed to copy encoder parameters to output stream" << std::endl;
		return;
	}
	out_stream->time_base = encCtxPtr->time_base;

	if (!(outFmtPtr->oformat->flags & AVFMT_NOFILE))
	{
		if (avio_open(&outFmtPtr->pb, output_filename, AVIO_FLAG_WRITE) < 0)
		{
			std::cout << "Could not open output file 『" << output_filename << "』" << std::endl;
			return;
		}
	}

	if (avformat_write_header(outFmtPtr.get(), nullptr) < 0)
	{
		std::cout << "Error occurred when opening output file" << std::endl;
		return;
	}

	// 分配并用 RAII 包装 packet/frame
	AVPacket* raw_pkt = av_packet_alloc();
	AVPacketPtr pktPtr(raw_pkt);
	AVFrame* raw_decoded = av_frame_alloc();
	AVFramePtr decodedPtr(raw_decoded);
	AVFrame* raw_yuv_full = av_frame_alloc();
	AVFramePtr yuvFullPtr(raw_yuv_full);
	AVFrame* raw_cropped = av_frame_alloc();
	AVFramePtr croppedPtr(raw_cropped);
	AVFrame* raw_conv_enc = av_frame_alloc();
	AVFramePtr convEncPtr(raw_conv_enc);

	if (!pktPtr || !decodedPtr || !yuvFullPtr || !croppedPtr || !convEncPtr)
	{
		std::cout << "Allocation failed" << std::endl;
		return;
	}

	// 为 yuv_full 分配 YUV420P 缓冲（full-size）
	yuvFullPtr->format = AV_PIX_FMT_YUV420P;
	yuvFullPtr->width = orig_w;
	yuvFullPtr->height = orig_h;
	if (av_frame_get_buffer(yuvFullPtr.get(), 32) < 0)
	{
		std::cout << "Failed to allocate yuv_full buffer" << std::endl;
		return;
	}

	// 为 cropped 分配 YUV420P 缓冲（new size）
	croppedPtr->format = AV_PIX_FMT_YUV420P;
	croppedPtr->width = new_w;
	croppedPtr->height = new_h;
	if (av_frame_get_buffer(croppedPtr.get(), 32) < 0)
	{
		std::cout << "Failed to allocate cropped buffer" << std::endl;
		return;
	}

	// 为 conv_for_enc 分配缓冲（当编码器像素格式 != YUV420P 时）
	bool enc_needs_conv = (encCtxPtr->pix_fmt != AV_PIX_FMT_YUV420P);
	convEncPtr->format = encCtxPtr->pix_fmt;
	convEncPtr->width = new_w;
	convEncPtr->height = new_h;
	if (enc_needs_conv)
	{
		if (av_frame_get_buffer(convEncPtr.get(), 32) < 0)
		{
			std::cout << "Failed to allocate conv_for_enc buffer" << std::endl;
			return;
		}
	}

	// sws 上下文使用 RAII
    SwsCtxPtr sws_to_yuv(nullptr);
    SwsCtxPtr sws_to_enc(nullptr);

	// 主循环：读取包 -> 解码 -> 转为 YUV420P(full) -> 中心裁剪 -> （必要时）转换为 encoder pixfmt -> 编码写出
	while (av_read_frame(inFmtPtr.get(), pktPtr.get()) >= 0)
	{
		if (pktPtr->stream_index != video_stream_index)
		{
			av_packet_unref(pktPtr.get());
			continue;
		}

		int ret = avcodec_send_packet(decCtxPtr.get(), pktPtr.get());
		av_packet_unref(pktPtr.get());
		if (ret < 0)
		{
			std::cout << "Error while sending a packet to the decoder" << std::endl;
			return;
		}

		while (ret >= 0)
		{
			ret = avcodec_receive_frame(decCtxPtr.get(), decodedPtr.get());
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
			if (ret < 0)
			{
				std::cout << "Error while receiving a frame from the decoder" << std::endl;
				return;
			}

			decodedPtr->pts = decodedPtr->best_effort_timestamp;

			// 转为 YUV420P full-size（如需）
			if (decodedPtr->format != AV_PIX_FMT_YUV420P || decodedPtr->width != orig_w || decodedPtr->height != orig_h)
			{
				if (!sws_to_yuv)
				{
					SwsContext* s = sws_getContext(decodedPtr->width, decodedPtr->height, static_cast<AVPixelFormat>(decodedPtr->format),
						orig_w, orig_h, AV_PIX_FMT_YUV420P,
						SWS_BILINEAR, nullptr, nullptr, nullptr);
					if (!s) { std::cout << "Failed to create sws_to_yuv" << std::endl; return; }
					sws_to_yuv.reset(s);
				}
				sws_scale(sws_to_yuv.get(),
					decodedPtr->data, decodedPtr->linesize, 0, decodedPtr->height,
					yuvFullPtr->data, yuvFullPtr->linesize);
			}
			else
			{
				// 逐平面按行拷贝
				for (int p = 0; p < 3; ++p)
				{
					int plane_h = (p == 0) ? orig_h : (orig_h / 2);
					int plane_w = (p == 0) ? orig_w : (orig_w / 2);
					for (int r = 0; r < plane_h; ++r)
					{
						memcpy(yuvFullPtr->data[p] + (size_t)r * yuvFullPtr->linesize[p],
							decodedPtr->data[p] + (size_t)r * decodedPtr->linesize[p],
							static_cast<size_t>(plane_w));
					}
				}
			}

			// 在 YUV420P full 上执行中心裁剪 -> cropped
			for (int r = 0; r < new_h; ++r)
			{
				uint8_t* dst = croppedPtr->data[0] + (size_t)r * croppedPtr->linesize[0];
				uint8_t* src = yuvFullPtr->data[0] + (size_t)(r + crop_top) * yuvFullPtr->linesize[0] + crop_left;
				memcpy(dst, src, static_cast<size_t>(new_w));
			}
			int crop_left_chroma = crop_left / 2;
			int crop_top_chroma = crop_top / 2;
			int new_w_chroma = new_w / 2;
			int new_h_chroma = new_h / 2;
			for (int r = 0; r < new_h_chroma; ++r)
			{
				uint8_t* dstu = croppedPtr->data[1] + (size_t)r * croppedPtr->linesize[1];
				uint8_t* srcu = yuvFullPtr->data[1] + (size_t)(r + crop_top_chroma) * yuvFullPtr->linesize[1] + crop_left_chroma;
				memcpy(dstu, srcu, static_cast<size_t>(new_w_chroma));
			}
			for (int r = 0; r < new_h_chroma; ++r)
			{
				uint8_t* dstv = croppedPtr->data[2] + (size_t)r * croppedPtr->linesize[2];
				uint8_t* srcv = yuvFullPtr->data[2] + (size_t)(r + crop_top_chroma) * yuvFullPtr->linesize[2] + crop_left_chroma;
				memcpy(dstv, srcv, static_cast<size_t>(new_w_chroma));
			}

			croppedPtr->pts = decodedPtr->pts;

			// 如果编码器像素格式不是 YUV420P，需要转换
			AVFrame* frame_to_send = croppedPtr.get();
			if (enc_needs_conv)
			{
				if (!sws_to_enc)
				{
					SwsContext* s = sws_getContext(new_w, new_h, AV_PIX_FMT_YUV420P,
						new_w, new_h, encCtxPtr->pix_fmt,
						SWS_BILINEAR, nullptr, nullptr, nullptr);
					if (!s) { std::cout << "Failed to create sws_to_enc" << std::endl; return; }
					sws_to_enc.reset(s);
				}
				sws_scale(sws_to_enc.get(),
					croppedPtr->data, croppedPtr->linesize, 0, new_h,
					convEncPtr->data, convEncPtr->linesize);
				convEncPtr->pts = croppedPtr->pts;
				frame_to_send = convEncPtr.get();
			}

			// 发送并接收编码器输出
			if (avcodec_send_frame(encCtxPtr.get(), frame_to_send) < 0)
			{
				std::cout << "Error while sending a frame to the encoder" << std::endl;
				return;
			}
			while (true)
			{
				int recv_ret = avcodec_receive_packet(encCtxPtr.get(), pktPtr.get());
				if (recv_ret == AVERROR(EAGAIN) || recv_ret == AVERROR_EOF) break;
				if (recv_ret < 0)
				{
					std::cout << "Error while receiving a packet from the encoder" << std::endl;
					return;
				}
				av_packet_rescale_ts(pktPtr.get(), encCtxPtr->time_base, out_stream->time_base);
				pktPtr->stream_index = out_stream->index;
				if (av_interleaved_write_frame(outFmtPtr.get(), pktPtr.get()) < 0)
				{
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
	while (avcodec_receive_frame(decCtxPtr.get(), decodedPtr.get()) >= 0)
	{
		decodedPtr->pts = decodedPtr->best_effort_timestamp;

		// 转为 YUV420P full-size（与上文相同）
		if (decodedPtr->format != AV_PIX_FMT_YUV420P || decodedPtr->width != orig_w || decodedPtr->height != orig_h)
		{
			if (!sws_to_yuv)
			{
				SwsContext* s = sws_getContext(decodedPtr->width, decodedPtr->height, static_cast<AVPixelFormat>(decodedPtr->format),
					orig_w, orig_h, AV_PIX_FMT_YUV420P,
					SWS_BILINEAR, nullptr, nullptr, nullptr);
				if (!s) { std::cout << "Failed to create sws_to_yuv" << std::endl; return; }
				sws_to_yuv.reset(s);
			}
			sws_scale(sws_to_yuv.get(),
				decodedPtr->data, decodedPtr->linesize, 0, decodedPtr->height,
				yuvFullPtr->data, yuvFullPtr->linesize);
		}
		else
		{
			for (int p = 0; p < 3; ++p)
			{
				int plane_h = (p == 0) ? orig_h : (orig_h / 2);
				int plane_w = (p == 0) ? orig_w : (orig_w / 2);
				for (int r = 0; r < plane_h; ++r)
				{
					memcpy(yuvFullPtr->data[p] + (size_t)r * yuvFullPtr->linesize[p],
						decodedPtr->data[p] + (size_t)r * decodedPtr->linesize[p],
						static_cast<size_t>(plane_w));
				}
			}
		}

		// 裁剪到 cropped
		for (int r = 0; r < new_h; ++r)
		{
			uint8_t* dst = croppedPtr->data[0] + (size_t)r * croppedPtr->linesize[0];
			uint8_t* src = yuvFullPtr->data[0] + (size_t)(r + crop_top) * yuvFullPtr->linesize[0] + crop_left;
			memcpy(dst, src, static_cast<size_t>(new_w));
		}
		int crop_left_ch = crop_left / 2;
		int crop_top_ch = crop_top / 2;
		int new_w_ch = new_w / 2;
		int new_h_ch = new_h / 2;
		for (int r = 0; r < new_h_ch; ++r)
		{
			uint8_t* dstu = croppedPtr->data[1] + (size_t)r * croppedPtr->linesize[1];
			uint8_t* srcu = yuvFullPtr->data[1] + (size_t)(r + crop_top_ch) * yuvFullPtr->linesize[1] + crop_left_ch;
			memcpy(dstu, srcu, static_cast<size_t>(new_w_ch));
		}
		for (int r = 0; r < new_h_ch; ++r)
		{
			uint8_t* dstv = croppedPtr->data[2] + (size_t)r * croppedPtr->linesize[2];
			uint8_t* srcv = yuvFullPtr->data[2] + (size_t)(r + crop_top_ch) * yuvFullPtr->linesize[2] + crop_left_ch;
			memcpy(dstv, srcv, static_cast<size_t>(new_w_ch));
		}

		croppedPtr->pts = decodedPtr->pts;

		AVFrame* frame_to_send = croppedPtr.get();
		if (enc_needs_conv)
		{
			if (!sws_to_enc)
			{
				SwsContext* s = sws_getContext(new_w, new_h, AV_PIX_FMT_YUV420P,
					new_w, new_h, encCtxPtr->pix_fmt,
					SWS_BILINEAR, nullptr, nullptr, nullptr);
				if (!s) { std::cout << "Failed to create sws_to_enc" << std::endl; return; }
				sws_to_enc.reset(s);
			}
			sws_scale(sws_to_enc.get(),
				croppedPtr->data, croppedPtr->linesize, 0, new_h,
				convEncPtr->data, convEncPtr->linesize);
			convEncPtr->pts = croppedPtr->pts;
			frame_to_send = convEncPtr.get();
		}

		if (avcodec_send_frame(encCtxPtr.get(), frame_to_send) < 0)
		{
			std::cout << "Error while sending a frame to the encoder during flush" << std::endl;
			return;
		}
		while (avcodec_receive_packet(encCtxPtr.get(), pktPtr.get()) == 0)
		{
			av_packet_rescale_ts(pktPtr.get(), encCtxPtr->time_base, out_stream->time_base);
			pktPtr->stream_index = out_stream->index;
			av_interleaved_write_frame(outFmtPtr.get(), pktPtr.get());
			av_packet_unref(pktPtr.get());
		}
	}

	// flush encoder
	avcodec_send_frame(encCtxPtr.get(), nullptr);
	while (avcodec_receive_packet(encCtxPtr.get(), pktPtr.get()) >= 0)
	{
		av_packet_rescale_ts(pktPtr.get(), encCtxPtr->time_base, out_stream->time_base);
		pktPtr->stream_index = out_stream->index;
		av_interleaved_write_frame(outFmtPtr.get(), pktPtr.get());
		av_packet_unref(pktPtr.get());
	}

	av_write_trailer(outFmtPtr.get());

	// 自动清理由 RAII 处理
}

#endif // DECANDENC_H_
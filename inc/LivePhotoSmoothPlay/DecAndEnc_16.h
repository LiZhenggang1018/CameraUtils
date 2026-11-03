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

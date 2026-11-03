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

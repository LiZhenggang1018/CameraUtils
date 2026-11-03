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

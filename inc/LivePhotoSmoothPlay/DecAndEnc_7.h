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

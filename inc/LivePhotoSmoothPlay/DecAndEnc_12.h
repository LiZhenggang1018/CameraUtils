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


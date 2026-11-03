
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

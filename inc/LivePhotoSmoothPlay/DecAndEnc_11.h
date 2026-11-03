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


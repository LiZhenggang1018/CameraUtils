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

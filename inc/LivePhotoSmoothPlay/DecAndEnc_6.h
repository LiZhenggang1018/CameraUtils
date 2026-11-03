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

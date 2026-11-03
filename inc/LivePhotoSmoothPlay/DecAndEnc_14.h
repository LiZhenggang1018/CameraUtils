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

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

	// 插入封面帧逻辑（保持原样但使用值初始化与 cout）
	{
		const char *yuv_path = "60.yuv";
		FILE *yuv_fp{};
		if (fopen_s(&yuv_fp, yuv_path, "rb") != 0)
		{
			yuv_fp = nullptr;
		}
		if (!yuv_fp)
		{
			std::cout << "警告：无法打开插入帧文件：" << yuv_path << " ，跳过插入" << std::endl;
		}
		else
		{
			int w = encoder_context->width;
			int h = encoder_context->height;
			if (w > 0 && h > 0)
			{
				AVFrame *yuv_frame{};
				yuv_frame = av_frame_alloc();
				if (yuv_frame)
				{
					yuv_frame->format = AV_PIX_FMT_YUV420P;
					yuv_frame->width = w;
					yuv_frame->height = h;
					if (av_frame_get_buffer(yuv_frame, 32) == 0)
					{
						size_t read{};
						for (int yy = 0; yy < h; ++yy)
						{
							read = fread(yuv_frame->data[0] + (size_t)yy * yuv_frame->linesize[0], 1, w, yuv_fp);
							if (read != (size_t)w)
							{
								break;
							}
						}
						for (int yy = 0; yy < h / 2; ++yy)
						{
							read = fread(yuv_frame->data[1] + (size_t)yy * yuv_frame->linesize[1], 1, w / 2, yuv_fp);
							if (read != (size_t)(w / 2))
							{
								break;
							}
						}
						for (int yy = 0; yy < h / 2; ++yy)
						{
							read = fread(yuv_frame->data[2] + (size_t)yy * yuv_frame->linesize[2], 1, w / 2, yuv_fp);
							if (read != (size_t)(w / 2))
							{
								break;
							}
						}

						if (last_decoded_pts == AV_NOPTS_VALUE)
						{
							last_decoded_pts = 0;
						}
						yuv_frame->pts = last_decoded_pts + 1;

						AVFrame *frame_to_send = yuv_frame;
						AVFrame *conv_frame{};
						SwsContext *local_sws{};
						if (encoder_context->pix_fmt != AV_PIX_FMT_YUV420P)
						{
							conv_frame = av_frame_alloc();
							if (conv_frame)
							{
								conv_frame->format = encoder_context->pix_fmt;
								conv_frame->width = w;
								conv_frame->height = h;
								if (av_frame_get_buffer(conv_frame, 32) == 0)
								{
									local_sws = sws_getContext(w, h, AV_PIX_FMT_YUV420P,
															   w, h, encoder_context->pix_fmt,
															   SWS_BILINEAR, nullptr, nullptr, nullptr);
									if (local_sws)
									{
										sws_scale(local_sws,
												  yuv_frame->data, yuv_frame->linesize, 0, h,
												  conv_frame->data, conv_frame->linesize);
										conv_frame->pts = yuv_frame->pts;
										frame_to_send = conv_frame;
									}
								}
							}
						}

						response = avcodec_send_frame(encoder_context, frame_to_send);
						if (response < 0)
						{
							std::cout << "Error while sending synthetic frame to encoder: " << response << std::endl;
						}
						else
						{
							while ((response = avcodec_receive_packet(encoder_context, packet)) >= 0)
							{
								av_packet_rescale_ts(packet, encoder_context->time_base, out_stream->time_base);
								packet->stream_index = out_stream->index;
								av_interleaved_write_frame(output_format_context, packet);
								av_packet_unref(packet);
							}
						}

						if (local_sws)
						{
							sws_freeContext(local_sws);
						}
						if (conv_frame)
						{
							av_frame_free(&conv_frame);
						}
					}

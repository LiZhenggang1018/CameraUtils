			return;
		}
		AVPacket *raw_out_pkt = av_packet_alloc();
		AVPacketPtr out_pkt(raw_out_pkt);
		while (avcodec_receive_packet(enc_ctx.get(), out_pkt.get()) == 0)
		{
			av_packet_rescale_ts(out_pkt.get(), enc_ctx->time_base, out_stream->time_base);
			out_pkt->stream_index = out_stream->index;
			av_interleaved_write_frame(out_fmt.get(), out_pkt.get());
			av_packet_unref(out_pkt.get());
		}

		frame_index += 1;
	}

	if (!inserted_replacements && !replaceYuvFiles.empty() && frame_index <= toReplaceYuvStartIdx)
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
#endif
			FilePtr rep_fp(raw_fp);
			if (!rep_fp)
			{
				std::cout << "Warning: cannot open replace YUV file: " << replacePath << std::endl;
				continue;
			}

			AVFrame *rep_frame = av_frame_alloc();
			AVFramePtr rep_frame_ptr(rep_frame);
			if (!rep_frame)
			{
				std::cout << "Warning: av_frame_alloc failed for replace frame: " << replacePath << std::endl;
				continue;
			}
			rep_frame->format = AV_PIX_FMT_YUV420P;
			rep_frame->width = static_cast<int>(enc_ctx->width);
			rep_frame->height = static_cast<int>(enc_ctx->height);
			if (av_frame_get_buffer(rep_frame, 32) < 0)
			{
				std::cout << "Warning: av_frame_get_buffer failed for: " << replacePath << std::endl;
				continue;
			}

			bool read_ok = true;
			for (int y = 0; y < rep_frame->height && read_ok; ++y)
			{
				size_t want = static_cast<size_t>(rep_frame->width);
				size_t got = fread(rep_frame->data[0] + static_cast<size_t>(y) * rep_frame->linesize[0], 1, want, rep_fp.get());
				if (got != want)
				{
					read_ok = false;
				}
			}
			for (int y = 0; y < rep_frame->height / 2 && read_ok; ++y)
			{
				size_t want = static_cast<size_t>(rep_frame->width / 2);
				size_t got = fread(rep_frame->data[1] + static_cast<size_t>(y) * rep_frame->linesize[1], 1, want, rep_fp.get());
				if (got != want)
				{
					read_ok = false;
				}
			}
			for (int y = 0; y < rep_frame->height / 2 && read_ok; ++y)
			{
				size_t want = static_cast<size_t>(rep_frame->width / 2);
				size_t got = fread(rep_frame->data[2] + static_cast<size_t>(y) * rep_frame->linesize[2], 1, want, rep_fp.get());
				if (got != want)
				{
					read_ok = false;
				}
			}
			if (!read_ok)
			{
				std::cout << "Warning: read YUV failed for: " << replacePath << std::endl;
				continue;
			}

			if (last_decoded_pts == AV_NOPTS_VALUE)
			{
				last_decoded_pts = 0;
			}
			last_decoded_pts += 1;
			rep_frame->pts = last_decoded_pts;

			AVFrame *frame_to_send = rep_frame;
			AVFrame *conv_frame = nullptr;
			SwsContext *conv_sws = nullptr;
			AVFramePtr conv_frame_ptr(nullptr);

			if (enc_ctx->pix_fmt != AV_PIX_FMT_YUV420P)
			{
				conv_frame = av_frame_alloc();
				conv_frame_ptr.reset(conv_frame);
				if (conv_frame)
				{
					conv_frame->format = enc_ctx->pix_fmt;
					conv_frame->width = rep_frame->width;
					conv_frame->height = rep_frame->height;
					if (av_frame_get_buffer(conv_frame, 32) == 0)
					{
						conv_sws = sws_getContext(rep_frame->width, rep_frame->height, AV_PIX_FMT_YUV420P,
												  rep_frame->width, rep_frame->height, enc_ctx->pix_fmt,
												  SWS_BILINEAR, nullptr, nullptr, nullptr);
						if (conv_sws)
						{
							sws_scale(conv_sws, rep_frame->data, rep_frame->linesize, 0, rep_frame->height,
									  conv_frame->data, conv_frame->linesize);
							conv_frame->pts = rep_frame->pts;
							frame_to_send = conv_frame;
						}
					}
				}
			}

			response = avcodec_send_frame(enc_ctx.get(), frame_to_send);
			if (response < 0)
			{
				std::cout << "Error sending appended replacement frame to encoder: " << response << std::endl;
			}
			else
			{
				AVPacket *raw_out_pkt = av_packet_alloc();
				AVPacketPtr out_pkt(raw_out_pkt);
				while (avcodec_receive_packet(enc_ctx.get(), out_pkt.get()) == 0)
				{
					av_packet_rescale_ts(out_pkt.get(), enc_ctx->time_base, out_stream->time_base);
					out_pkt->stream_index = out_stream->index;
					av_interleaved_write_frame(out_fmt.get(), out_pkt.get());
					av_packet_unref(out_pkt.get());
				}
			}

			if (conv_sws)
			{
				sws_freeContext(conv_sws);
			}
		}
	}

	avcodec_send_frame(enc_ctx.get(), nullptr);
	while (true)
	{

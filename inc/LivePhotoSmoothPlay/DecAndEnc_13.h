				// 生成 JPEG（可选）
				if (jpeg_enc_ctx && jpeg_pkt)
				{
					AVFrame *frame_for_jpeg{};
					if (out_frame->format != jpeg_enc_ctx->pix_fmt)
					{
						if (!jpeg_conv_frame)
						{
							jpeg_conv_frame = av_frame_alloc();
							if (jpeg_conv_frame)
							{
								jpeg_conv_frame->format = jpeg_enc_ctx->pix_fmt;
								jpeg_conv_frame->width = width;
								jpeg_conv_frame->height = height;
								jpeg_conv_frame->color_range = jpeg_enc_ctx->color_range;
								if (av_frame_get_buffer(jpeg_conv_frame, 32) < 0)
								{
									av_frame_free(&jpeg_conv_frame);
									jpeg_conv_frame = nullptr;
								}
							}
						}
						if (jpeg_conv_frame)
						{
							if (!jpeg_sws_ctx)
							{
								jpeg_sws_ctx_raw = sws_getContext(width, height, static_cast<AVPixelFormat>(out_frame->format),
																  width, height, jpeg_enc_ctx->pix_fmt,
																  SWS_BILINEAR, nullptr, nullptr, nullptr);
								if (!jpeg_sws_ctx_raw)
								{
									std::cout << "无法创建 jpeg sws 上下文" << std::endl;
									// 继续，不影响 YUV 写出
								}
								else
								{
									jpeg_sws_ctx.reset(jpeg_sws_ctx_raw);
								}
							}
							if (jpeg_sws_ctx)
							{
								sws_scale(jpeg_sws_ctx.get(),
										  out_frame->data, out_frame->linesize,
										  0, height,
										  jpeg_conv_frame->data, jpeg_conv_frame->linesize);
								jpeg_conv_frame->color_range = jpeg_enc_ctx->color_range;
								frame_for_jpeg = jpeg_conv_frame;
							}
						}
					}
					else
					{
						frame_for_jpeg = out_frame;
					}

					if (frame_for_jpeg)
					{
						frame_for_jpeg->pts = frame_index;
						if (avcodec_send_frame(jpeg_enc_ctx, frame_for_jpeg) == 0)
						{
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
								break;
							}
						}
					}
				}

				++frame_index;
			}
		}
		av_packet_unref(pkt.get());
	}

	// 发送 NULL 刷新解码器缓冲
	avcodec_send_packet(dec_ctx.get(), nullptr);
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
			sws_scale(sws_ctx.get(), frame->data, frame->linesize, 0, height, yuv->data, yuv->linesize);
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

		// 也为刷新阶段的帧生成 JPEG（与上文相同的逻辑）
		if (jpeg_enc_ctx && jpeg_pkt)
		{
			AVFrame *frame_for_jpeg{};
			if (out_frame->format != jpeg_enc_ctx->pix_fmt)
			{
				if (jpeg_conv_frame && jpeg_sws_ctx)
				{
					sws_scale(jpeg_sws_ctx.get(),
							  out_frame->data, out_frame->linesize,
							  0, height,
							  jpeg_conv_frame->data, jpeg_conv_frame->linesize);
					frame_for_jpeg = jpeg_conv_frame;
				}
			}
			else
			{
				frame_for_jpeg = out_frame;
			}

			if (frame_for_jpeg)
			{
				frame_for_jpeg->pts = 0;
				if (avcodec_send_frame(jpeg_enc_ctx, frame_for_jpeg) == 0)
				{
					while (avcodec_receive_packet(jpeg_enc_ctx, jpeg_pkt.get()) == 0)

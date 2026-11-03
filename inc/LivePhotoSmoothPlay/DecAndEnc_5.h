					response = av_interleaved_write_frame(output_format_context, packetPtr.get());
					is_av_interleaved_write_frame_success = true;
					av_interleaved_write_frame_cnt++;
					if (response < 0)
					{
						std::cout << "Error while writing packet to output file" << std::endl;
						av_packet_unref(packetPtr.get());
						return;
					}
					av_packet_unref(packetPtr.get());
				}
			}
		}
		av_packet_unref(packetPtr.get());
		if (!is_av_interleaved_write_frame_success)
		{
			PV("av_interleaved_write_frame fails!");
		}
	}

	auto skipFrames = av_read_frame_cnt - av_interleaved_write_frame_cnt;
	PV("total:", av_read_frame_cnt, "; write: ", av_interleaved_write_frame_cnt, "; skip: ", skipFrames);

	// Flush decoder
	avcodec_send_packet(decoder_context, nullptr);
	while (avcodec_receive_frame(decoder_context, framePtr.get()) >= 0)
	{
		framePtr->pts = framePtr->best_effort_timestamp;
		last_decoded_pts = framePtr->pts;

		avcodec_send_frame(encoder_context, framePtr.get());
		while (avcodec_receive_packet(encoder_context, packetPtr.get()) >= 0)
		{
			av_interleaved_write_frame(output_format_context, packetPtr.get());
			av_packet_unref(packetPtr.get());
		}
	}

	// 将 new10.yuv .. new1.yuv 依次插入到视频结尾
	{
		if (last_decoded_pts == AV_NOPTS_VALUE)
		{
			last_decoded_pts = 0;
		}
		int64_t pts_counter = last_decoded_pts;

		const int w = encoder_context->width;
		const int h = encoder_context->height;
		const enum AVPixelFormat src_pix_fmt = AV_PIX_FMT_YUV420P;

		for (int n = 10; n >= 1; --n)
		{
			std::string fname = "new" + std::to_string(n) + ".yuv";
			FILE *in_fp{};
			if (fopen_s(&in_fp, fname.c_str(), "rb") != 0)
			{
				in_fp = nullptr;
			}
			if (!in_fp)
			{
				std::cout << "警告：无法打开插入帧文件：" << fname << " ，跳过该帧" << std::endl;
				continue;
			}

			AVFrame *ins_frame = av_frame_alloc();
			AVFramePtr insFramePtr(ins_frame);
			if (!insFramePtr)
			{
				std::cout << "警告：av_frame_alloc 失败，跳过 " << fname << std::endl;
				if (in_fp)
				{
					fclose(in_fp);
				}
				continue;
			}
			insFramePtr->format = src_pix_fmt;
			insFramePtr->width = w;
			insFramePtr->height = h;
			if (av_frame_get_buffer(insFramePtr.get(), 32) < 0)
			{
				std::cout << "警告：av_frame_get_buffer 失败，跳过 " << fname << std::endl;
				av_frame_free(&ins_frame);
				if (in_fp)
				{
					fclose(in_fp);
				}
				continue;
			}

			bool read_ok = true;
			for (int yy = 0; yy < h; ++yy)
			{
				size_t want = static_cast<size_t>(w);
				size_t got = fread(insFramePtr->data[0] + static_cast<size_t>(yy) * insFramePtr->linesize[0], 1, want, in_fp);
				if (got != want)
				{
					read_ok = false;
					break;
				}
			}
			for (int yy = 0; yy < h / 2 && read_ok; ++yy)
			{
				size_t want = static_cast<size_t>(w / 2);
				size_t got = fread(insFramePtr->data[1] + static_cast<size_t>(yy) * insFramePtr->linesize[1], 1, want, in_fp);
				if (got != want)
				{
					read_ok = false;
					break;
				}
			}
			for (int yy = 0; yy < h / 2 && read_ok; ++yy)
			{
				size_t want = static_cast<size_t>(w / 2);
				size_t got = fread(insFramePtr->data[2] + static_cast<size_t>(yy) * insFramePtr->linesize[2], 1, want, in_fp);
				if (got != want)
				{
					read_ok = false;
					break;
				}
			}
			if (in_fp)
			{
				fclose(in_fp);
			}

			if (!read_ok)
			{
				std::cout << "警告：从 " << fname << " 读取数据失败，跳过" << std::endl;
				continue;
			}

			pts_counter += 1;
			insFramePtr->pts = pts_counter;

			AVFrame *frame_to_send = insFramePtr.get();
			AVFrame *conv{};
			SwsContext *local_sws{};
			AVFramePtr convPtr(nullptr);
			if (encoder_context->pix_fmt != src_pix_fmt)
			{
				conv = av_frame_alloc();
				convPtr.reset(conv);
				if (!convPtr)
				{
					std::cout << "警告：无法分配 conv frame，直接发送原始帧（可能不被编码器接受）: " << fname << std::endl;
				}
				else
				{
					convPtr->format = encoder_context->pix_fmt;
					convPtr->width = w;

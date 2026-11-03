		{
			std::cout << "Failed to allocate conv_for_enc buffer" << std::endl;
			return;
		}
	}

	// sws 上下文使用 RAII
    SwsCtxPtr sws_to_yuv(nullptr);
    SwsCtxPtr sws_to_enc(nullptr);

	// 主循环：读取包 -> 解码 -> 转为 YUV420P(full) -> 中心裁剪 -> （必要时）转换为 encoder pixfmt -> 编码写出
	while (av_read_frame(inFmtPtr.get(), pktPtr.get()) >= 0)
	{
		if (pktPtr->stream_index != video_stream_index)
		{
			av_packet_unref(pktPtr.get());
			continue;
		}

		int ret = avcodec_send_packet(decCtxPtr.get(), pktPtr.get());
		av_packet_unref(pktPtr.get());
		if (ret < 0)
		{
			std::cout << "Error while sending a packet to the decoder" << std::endl;
			return;
		}

		while (ret >= 0)
		{
			ret = avcodec_receive_frame(decCtxPtr.get(), decodedPtr.get());
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
			if (ret < 0)
			{
				std::cout << "Error while receiving a frame from the decoder" << std::endl;
				return;
			}

			decodedPtr->pts = decodedPtr->best_effort_timestamp;

			// 转为 YUV420P full-size（如需）
			if (decodedPtr->format != AV_PIX_FMT_YUV420P || decodedPtr->width != orig_w || decodedPtr->height != orig_h)
			{
				if (!sws_to_yuv)
				{
					SwsContext* s = sws_getContext(decodedPtr->width, decodedPtr->height, static_cast<AVPixelFormat>(decodedPtr->format),
						orig_w, orig_h, AV_PIX_FMT_YUV420P,
						SWS_BILINEAR, nullptr, nullptr, nullptr);
					if (!s) { std::cout << "Failed to create sws_to_yuv" << std::endl; return; }
					sws_to_yuv.reset(s);
				}
				sws_scale(sws_to_yuv.get(),
					decodedPtr->data, decodedPtr->linesize, 0, decodedPtr->height,
					yuvFullPtr->data, yuvFullPtr->linesize);
			}
			else
			{
				// 逐平面按行拷贝
				for (int p = 0; p < 3; ++p)
				{
					int plane_h = (p == 0) ? orig_h : (orig_h / 2);
					int plane_w = (p == 0) ? orig_w : (orig_w / 2);
					for (int r = 0; r < plane_h; ++r)
					{
						memcpy(yuvFullPtr->data[p] + (size_t)r * yuvFullPtr->linesize[p],
							decodedPtr->data[p] + (size_t)r * decodedPtr->linesize[p],
							static_cast<size_t>(plane_w));
					}
				}
			}

			// 在 YUV420P full 上执行中心裁剪 -> cropped
			for (int r = 0; r < new_h; ++r)
			{
				uint8_t* dst = croppedPtr->data[0] + (size_t)r * croppedPtr->linesize[0];
				uint8_t* src = yuvFullPtr->data[0] + (size_t)(r + crop_top) * yuvFullPtr->linesize[0] + crop_left;
				memcpy(dst, src, static_cast<size_t>(new_w));
			}
			int crop_left_chroma = crop_left / 2;
			int crop_top_chroma = crop_top / 2;
			int new_w_chroma = new_w / 2;
			int new_h_chroma = new_h / 2;
			for (int r = 0; r < new_h_chroma; ++r)
			{
				uint8_t* dstu = croppedPtr->data[1] + (size_t)r * croppedPtr->linesize[1];
				uint8_t* srcu = yuvFullPtr->data[1] + (size_t)(r + crop_top_chroma) * yuvFullPtr->linesize[1] + crop_left_chroma;
				memcpy(dstu, srcu, static_cast<size_t>(new_w_chroma));
			}
			for (int r = 0; r < new_h_chroma; ++r)
			{
				uint8_t* dstv = croppedPtr->data[2] + (size_t)r * croppedPtr->linesize[2];
				uint8_t* srcv = yuvFullPtr->data[2] + (size_t)(r + crop_top_chroma) * yuvFullPtr->linesize[2] + crop_left_chroma;
				memcpy(dstv, srcv, static_cast<size_t>(new_w_chroma));
			}

			croppedPtr->pts = decodedPtr->pts;

			// 如果编码器像素格式不是 YUV420P，需要转换
			AVFrame* frame_to_send = croppedPtr.get();
			if (enc_needs_conv)
			{
				if (!sws_to_enc)
				{
					SwsContext* s = sws_getContext(new_w, new_h, AV_PIX_FMT_YUV420P,
						new_w, new_h, encCtxPtr->pix_fmt,
						SWS_BILINEAR, nullptr, nullptr, nullptr);
					if (!s) { std::cout << "Failed to create sws_to_enc" << std::endl; return; }
					sws_to_enc.reset(s);
				}
				sws_scale(sws_to_enc.get(),
					croppedPtr->data, croppedPtr->linesize, 0, new_h,
					convEncPtr->data, convEncPtr->linesize);
				convEncPtr->pts = croppedPtr->pts;
				frame_to_send = convEncPtr.get();
			}

			// 发送并接收编码器输出
			if (avcodec_send_frame(encCtxPtr.get(), frame_to_send) < 0)
			{
				std::cout << "Error while sending a frame to the encoder" << std::endl;
				return;
			}
			while (true)
			{
				int recv_ret = avcodec_receive_packet(encCtxPtr.get(), pktPtr.get());
				if (recv_ret == AVERROR(EAGAIN) || recv_ret == AVERROR_EOF) break;
				if (recv_ret < 0)
				{
					std::cout << "Error while receiving a packet from the encoder" << std::endl;
					return;
				}
				av_packet_rescale_ts(pktPtr.get(), encCtxPtr->time_base, out_stream->time_base);
				pktPtr->stream_index = out_stream->index;
				if (av_interleaved_write_frame(outFmtPtr.get(), pktPtr.get()) < 0)
				{
					std::cout << "Error while writing packet to output file" << std::endl;
					av_packet_unref(pktPtr.get());
					return;
				}
				av_packet_unref(pktPtr.get());
			}
		} // end receive_frame loop
	} // end read_frame loop

	// flush decoder
	avcodec_send_packet(decCtxPtr.get(), nullptr);
	while (avcodec_receive_frame(decCtxPtr.get(), decodedPtr.get()) >= 0)
	{
		decodedPtr->pts = decodedPtr->best_effort_timestamp;

		// 转为 YUV420P full-size（与上文相同）

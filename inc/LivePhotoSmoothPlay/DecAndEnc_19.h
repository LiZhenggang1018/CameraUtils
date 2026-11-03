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

		// 裁剪到 cropped
		for (int r = 0; r < new_h; ++r)
		{
			uint8_t* dst = croppedPtr->data[0] + (size_t)r * croppedPtr->linesize[0];
			uint8_t* src = yuvFullPtr->data[0] + (size_t)(r + crop_top) * yuvFullPtr->linesize[0] + crop_left;
			memcpy(dst, src, static_cast<size_t>(new_w));
		}
		int crop_left_ch = crop_left / 2;
		int crop_top_ch = crop_top / 2;
		int new_w_ch = new_w / 2;
		int new_h_ch = new_h / 2;
		for (int r = 0; r < new_h_ch; ++r)
		{
			uint8_t* dstu = croppedPtr->data[1] + (size_t)r * croppedPtr->linesize[1];
			uint8_t* srcu = yuvFullPtr->data[1] + (size_t)(r + crop_top_ch) * yuvFullPtr->linesize[1] + crop_left_ch;
			memcpy(dstu, srcu, static_cast<size_t>(new_w_ch));
		}
		for (int r = 0; r < new_h_ch; ++r)
		{
			uint8_t* dstv = croppedPtr->data[2] + (size_t)r * croppedPtr->linesize[2];
			uint8_t* srcv = yuvFullPtr->data[2] + (size_t)(r + crop_top_ch) * yuvFullPtr->linesize[2] + crop_left_ch;
			memcpy(dstv, srcv, static_cast<size_t>(new_w_ch));
		}

		croppedPtr->pts = decodedPtr->pts;

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

		if (avcodec_send_frame(encCtxPtr.get(), frame_to_send) < 0)
		{
			std::cout << "Error while sending a frame to the encoder during flush" << std::endl;
			return;
		}
		while (avcodec_receive_packet(encCtxPtr.get(), pktPtr.get()) == 0)
		{
			av_packet_rescale_ts(pktPtr.get(), encCtxPtr->time_base, out_stream->time_base);
			pktPtr->stream_index = out_stream->index;
			av_interleaved_write_frame(outFmtPtr.get(), pktPtr.get());
			av_packet_unref(pktPtr.get());
		}
	}

	// flush encoder
	avcodec_send_frame(encCtxPtr.get(), nullptr);
	while (avcodec_receive_packet(encCtxPtr.get(), pktPtr.get()) >= 0)
	{
		av_packet_rescale_ts(pktPtr.get(), encCtxPtr->time_base, out_stream->time_base);
		pktPtr->stream_index = out_stream->index;
		av_interleaved_write_frame(outFmtPtr.get(), pktPtr.get());
		av_packet_unref(pktPtr.get());
	}

	av_write_trailer(outFmtPtr.get());

	// 自动清理由 RAII 处理
}

#endif // DECANDENC_H_
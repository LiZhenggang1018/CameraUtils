	// 分配视频帧数据
	if (av_frame_get_buffer(frame, 32) < 0)
	{
		std::cerr << "无法分配视频帧数据\n";
		return -1;
	}

	// 生成并编码每一帧
	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		// 用 memset 零初始化 pkt，确保所有字段为默认值
		memset(&pkt, 0, sizeof(pkt));
		pkt.data = nullptr;
		pkt.size = 0;

		// 确保帧数据可写
		if (av_frame_make_writable(frame) < 0)
		{
			std::cerr << "帧数据不可写\n";
			return -1;
		}

		// 生成 YUV 帧数据
		GenSimpleYuvFrames(frame->data[0], i);

		frame->pts = i;

		// 发送帧到编码器
		int ret = avcodec_send_frame(codec_context, frame);
		if (ret < 0)
		{
			std::cerr << "发送帧到编码器时出错\n";
			return -1;
		}

		// 接收编码后的包
		ret = avcodec_receive_packet(codec_context, &pkt);
		if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
		{
			std::cerr << "接收编码包时出错\n";
			return -1;
		}

		if (ret >= 0)
		{
			pkt.stream_index = video_stream->index;
			av_packet_rescale_ts(&pkt, codec_context->time_base, video_stream->time_base);
			pkt.pos = -1;

			// 写入编码包到输出文件
			if (av_interleaved_write_frame(format_context, &pkt) < 0)
			{
				std::cerr << "写入帧时出错\n";
				return -1;
			}
			av_packet_unref(&pkt);
		}
	}

	// 写入文件尾部
	av_write_trailer(format_context);

	// 释放资源
	avcodec_free_context(&codec_context);
	av_frame_free(&frame);
	avio_closep(&format_context->pb);
	avformat_free_context(format_context);

	return 0;
}

#endif // GENMP4_H_
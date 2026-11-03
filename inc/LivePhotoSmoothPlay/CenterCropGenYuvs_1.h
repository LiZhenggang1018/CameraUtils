        std::vector<unsigned char> cropY(cropYSize);
        std::vector<unsigned char> cropU(cropUVSize);
        std::vector<unsigned char> cropV(cropUVSize);

        // 裁剪Y分量
        for (int yy = 0; yy < cropH; yy++)
        {
            int srcY = startY + yy;
            int dstY = yy;

            for (int xx = 0; xx < cropW; xx++)
            {
                int srcX = startX + xx;
                int dstX = xx;

                cropY[dstY * cropW + dstX] = Y[static_cast<size_t>(srcY) * static_cast<size_t>(w) + static_cast<size_t>(srcX)];
            }
        }

        // 裁剪U分量
        for (int yy = 0; yy < cropH_UV; yy++)
        {
            int srcY = startY_UV + yy;
            int dstY = yy;

            for (int xx = 0; xx < cropW_UV; xx++)
            {
                int srcX = startX_UV + xx;
                int dstX = xx;

                cropU[dstY * cropW_UV + dstX] = U[srcY * (w / 2) + srcX];
            }
        }

        // 裁剪V分量
        for (int yy = 0; yy < cropH_UV; yy++)
        {
            int srcY = startY_UV + yy;
            int dstY = yy;

            for (int xx = 0; xx < cropW_UV; xx++)
            {
                int srcX = startX_UV + xx;
                int dstX = xx;

                cropV[dstY * cropW_UV + dstX] = V[srcY * (w / 2) + srcX];
            }
        }

        // 生成输出文件名（使用 filesystem 拼接）
        fs::path yuvPath = out_path / ("crop" + std::to_string(n) + ".yuv");
        std::ofstream outputFile(yuvPath.string(), std::ios::binary);

        if (!outputFile.is_open())
        {
            std::cerr << "Error: Cannot create output file: " << yuvPath.string() << std::endl;
            continue;
        }

        // 创建全尺寸缓冲区并初始化（先填充默认值）
        std::vector<unsigned char> fullCropY(w * h, 0);       // Y分量，填充0（黑色）
        std::vector<unsigned char> fullCropU(w * h / 4, 128); // U分量，填充128（中性灰）
        std::vector<unsigned char> fullCropV(w * h / 4, 128); // V分量，填充128（中性灰）

        // 将裁剪后的数据复制到中心位置（保留作为回退方案）
        for (int yy = 0; yy < cropH; yy++)
        {
            for (int xx = 0; xx < cropW; xx++)
            {
                fullCropY[(startY + yy) * w + (startX + xx)] = cropY[yy * cropW + xx];
            }
        }

        for (int yy = 0; yy < cropH_UV; yy++)
        {
            for (int xx = 0; xx < cropW_UV; xx++)
            {
                fullCropU[(startY_UV + yy) * (w / 2) + (startX_UV + xx)] = cropU[yy * cropW_UV + xx];
                fullCropV[(startY_UV + yy) * (w / 2) + (startX_UV + xx)] = cropV[yy * cropW_UV + xx];
            }
        }

        // 新增逻辑：将裁剪的中心区域放大（resize）填满整个画幅，替代黑边填充
        bool resized_ok = false;
        try
        {
            if (cropYSize > 0 && cropUVSize > 0)
            {
                // Y 分量 resize 到 full size (h x w)
                cv::Mat cropYMat(cropH, cropW, CV_8UC1, cropY.data());
                cv::Mat resizedYMat;
                cv::resize(cropYMat, resizedYMat, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);

                // UV 分量 resize 到 full UV size (h/2 x w/2)
                cv::Mat cropUMat(cropH_UV, cropW_UV, CV_8UC1, cropU.data());
                cv::Mat resizedUMat;
                cv::resize(cropUMat, resizedUMat, cv::Size(w / 2, h / 2), 0, 0, cv::INTER_LINEAR);

                cv::Mat cropVMat(cropH_UV, cropW_UV, CV_8UC1, cropV.data());
                cv::Mat resizedVMat;
                cv::resize(cropVMat, resizedVMat, cv::Size(w / 2, h / 2), 0, 0, cv::INTER_LINEAR);

                // 检查尺寸并拷贝数据到 full buffer
                if (resizedYMat.total() == static_cast<size_t>(w) * h &&
                    resizedUMat.total() == static_cast<size_t>(w / 2) * (h / 2) &&
                    resizedVMat.total() == static_cast<size_t>(w / 2) * (h / 2))
                {

                    std::memcpy(fullCropY.data(), resizedYMat.data, fullCropY.size());
                    std::memcpy(fullCropU.data(), resizedUMat.data, fullCropU.size());
                    std::memcpy(fullCropV.data(), resizedVMat.data, fullCropV.size());
                    resized_ok = true;
                }
            }
        }
        catch (const cv::Exception &e)
        {
            std::cerr << "Warning: OpenCV exception during resize: " << e.what() << std::endl;
            resized_ok = false;
        }
        catch (...)
        {
            std::cerr << "Warning: Unknown exception during resize" << std::endl;
            resized_ok = false;
        }

        if (!resized_ok)
        {
            // 如果 resize 失败，fullCrop 保持原先中心放置的方式（已在上面完成）
            std::cerr << "Info: Using centered crop (no upscale) for " << yuvPath.string() << std::endl;
        }

        // 写入文件：先Y，再U，再V
        outputFile.write(reinterpret_cast<const char *>(fullCropY.data()), fullCropY.size());
        outputFile.write(reinterpret_cast<const char *>(fullCropU.data()), fullCropU.size());
        outputFile.write(reinterpret_cast<const char *>(fullCropV.data()), fullCropV.size());

        outputFile.close();

        // 同时生成对应的 JPG（将填充后的全尺寸 YUV 拼接为 I420 格式并转换为 BGR 后保存）
        try
        {
            size_t totalSize = static_cast<size_t>(w) * h * 3 / 2;
            std::vector<unsigned char> frameBuf;
            frameBuf.reserve(totalSize);
            // 按 I420 (Y U V) 顺序拼接
            frameBuf.insert(frameBuf.end(), fullCropY.begin(), fullCropY.end());
            frameBuf.insert(frameBuf.end(), fullCropU.begin(), fullCropU.end());
            frameBuf.insert(frameBuf.end(), fullCropV.begin(), fullCropV.end());


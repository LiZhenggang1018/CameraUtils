    if (!yuv1.pData || !yuv2.pData) {
        throw std::invalid_argument("YUV data pointers cannot be null");
    }

    // 计算YUV420文件大小
    int w = yuv1.width;
    int h = yuv1.height;
    int stride = yuv1.stride;

    size_t ySize = static_cast<size_t>(stride) * h;
    size_t uvSize = static_cast<size_t>(stride / 2) * (h / 2);
    size_t totalSize = ySize + uvSize * 2;  // Y + U + V

    // 验证数据大小
    if (stride < w) {
        throw std::invalid_argument("stride must be >= width");
    }

    // 创建输出缓冲区
    std::vector<unsigned char> outData(totalSize);

    // 计算混合比例
    double yuv2AlphaRatio = 1.0 - yuv1AlphaRatio;

    // 合并Y分量
    size_t yOffset = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t idx = yOffset + x;
            double mixed = yuv1.pData[idx] * yuv1AlphaRatio +
                yuv2.pData[idx] * yuv2AlphaRatio;
            outData[idx] = static_cast<unsigned char>(std::clamp(mixed, 0.0, 255.0));
        }
        yOffset += stride;
    }

    // 计算UV分量的尺寸和偏移
    size_t uStart = ySize;
    size_t vStart = ySize + uvSize;
    int uvWidth = w / 2;
    int uvHeight = h / 2;
    int uvStride = stride / 2;

    // 合并U分量
    size_t uOffset = 0;
    for (int y = 0; y < uvHeight; ++y) {
        for (int x = 0; x < uvWidth; ++x) {
            size_t idx = uStart + uOffset + x;
            double mixed = yuv1.pData[idx] * yuv1AlphaRatio +
                yuv2.pData[idx] * yuv2AlphaRatio;
            outData[idx] = static_cast<unsigned char>(std::clamp(mixed, 0.0, 255.0));
        }
        uOffset += uvStride;
    }

    // 合并V分量
    size_t vOffset = 0;
    for (int y = 0; y < uvHeight; ++y) {
        for (int x = 0; x < uvWidth; ++x) {
            size_t idx = vStart + vOffset + x;
            double mixed = yuv1.pData[idx] * yuv1AlphaRatio +
                yuv2.pData[idx] * yuv2AlphaRatio;
            outData[idx] = static_cast<unsigned char>(std::clamp(mixed, 0.0, 255.0));
        }
        vOffset += uvStride;
    }

    // 写入输出文件
    std::ofstream outFile(outYuv.data(), std::ios::binary);
    if (!outFile.is_open()) {
        throw std::runtime_error("Cannot create output file: " + std::string(outYuv));
    }

    outFile.write(reinterpret_cast<const char*>(outData.data()), totalSize);
    outFile.close();
}

// OpenCV实现，从内存YUV数据混合
inline void AlphaMergeYuvsOpenCV(const YuvInfo& yuv1, const YuvInfo& yuv2,double yuv1AlphaRatio, std::string_view outYuv) {

    // 参数验证
    if (yuv1.width <= 0 || yuv1.height <= 0 || yuv1.stride <= 0 ||
        yuv2.width <= 0 || yuv2.height <= 0 || yuv2.stride <= 0) {
        throw std::invalid_argument("Invalid YuvInfo parameters");
    }

    // 确保两个YUV数据尺寸一致
    if (yuv1.width != yuv2.width || yuv1.height != yuv2.height ||
        yuv1.stride != yuv2.stride) {
        throw std::invalid_argument("YUV data dimensions must match");
    }

    if (yuv1AlphaRatio < 0.0 || yuv1AlphaRatio > 1.0) {
        throw std::invalid_argument("yuv1AlphaRatio must be between 0.0 and 1.0");
    }

    if (!yuv1.pData || !yuv2.pData) {
        throw std::invalid_argument("YUV data pointers cannot be null");
    }

    // 计算YUV420文件大小
    int w = yuv1.width;
    int h = yuv1.height;
    int stride = yuv1.stride;

    size_t ySize = static_cast<size_t>(stride) * h;
    size_t uvSize = static_cast<size_t>(stride / 2) * (h / 2);
    size_t totalSize = ySize + uvSize * 2;

    // 使用OpenCV处理YUV数据
    // 创建OpenCV矩阵表示YUV数据（I420格式），注意步长参数
    cv::Mat yuvMat1(h + h / 2, stride, CV_8UC1, yuv1.pData);
    cv::Mat yuvMat2(h + h / 2, stride, CV_8UC1, yuv2.pData);

    // 提取Y、U、V平面
    cv::Mat yPlane1 = yuvMat1.rowRange(0, h);
    cv::Mat uvPlane1 = yuvMat1.rowRange(h, h + h / 2);

    cv::Mat yPlane2 = yuvMat2.rowRange(0, h);
    cv::Mat uvPlane2 = yuvMat2.rowRange(h, h + h / 2);

    // 分离UV平面为U和V
    cv::Mat uvChannels1[2], uvChannels2[2];

    // 重新排列UV交织的数据
    cv::Mat uvInterleaved1 = uvPlane1.reshape(2, h / 2);
    cv::Mat uvInterleaved2 = uvPlane2.reshape(2, h / 2);

    cv::split(uvInterleaved1, uvChannels1);
    cv::split(uvInterleaved2, uvChannels2);

    cv::Mat uPlane1 = uvChannels1[0];
    cv::Mat vPlane1 = uvChannels1[1];
    cv::Mat uPlane2 = uvChannels2[0];
    cv::Mat vPlane2 = uvChannels2[1];

    // 进行alpha混合
    cv::Mat yMixed, uMixed, vMixed;

    cv::addWeighted(yPlane1, yuv1AlphaRatio, yPlane2, 1.0 - yuv1AlphaRatio, 0.0, yMixed);
    cv::addWeighted(uPlane1, yuv1AlphaRatio, uPlane2, 1.0 - yuv1AlphaRatio, 0.0, uMixed);
    cv::addWeighted(vPlane1, yuv1AlphaRatio, vPlane2, 1.0 - yuv1AlphaRatio, 0.0, vMixed);

    // 重新合并UV平面
    cv::Mat uvChannelsMixed[2] = { uMixed, vMixed };
    cv::Mat uvMixed;
    cv::merge(uvChannelsMixed, 2, uvMixed);

    // 转换回交错的UV格式
    uvMixed = uvMixed.reshape(1, h / 2);

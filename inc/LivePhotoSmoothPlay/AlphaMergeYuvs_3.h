
    // 创建输出YUV矩阵
    cv::Mat outYuvMat(h + h / 2, stride, CV_8UC1);

    // 复制Y平面
    cv::Mat yOut = outYuvMat.rowRange(0, h);
    yMixed.copyTo(yOut);

    // 复制UV平面
    cv::Mat uvOut = outYuvMat.rowRange(h, h + h / 2);
    uvMixed.copyTo(uvOut);

    // 写入输出文件
    std::ofstream outFile(outYuv.data(), std::ios::binary);
    if (!outFile.is_open()) {
        throw std::runtime_error("Cannot create output file: " + std::string(outYuv));
    }

    outFile.write(reinterpret_cast<const char*>(outYuvMat.data), outYuvMat.total() * outYuvMat.elemSize());
    outFile.close();
}

// 辅助函数：从文件加载YUV数据到YuvInfo结构
inline YuvInfo LoadYuvFromFile(const std::string& filename, int width, int height, int stride) {
    // 计算YUV420文件大小
    size_t ySize = static_cast<size_t>(stride) * height;
    size_t uvSize = static_cast<size_t>(stride / 2) * (height / 2);
    size_t totalSize = ySize + uvSize * 2;

    // 读取文件
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    // 分配内存
    unsigned char* data = new unsigned char[totalSize];

    // 读取数据
    file.read(reinterpret_cast<char*>(data), totalSize);
    if (file.gcount() != static_cast<std::streamsize>(totalSize)) {
        delete[] data;
        throw std::runtime_error("File size mismatch for: " + filename);
    }

    file.close();

    // 返回YuvInfo结构
    return YuvInfo{ data, width, height, stride };
}

// 辅助函数：释放YuvInfo中的内存
inline void FreeYuvInfo(YuvInfo& yuvInfo) {
    if (yuvInfo.pData) {
        delete[] yuvInfo.pData;
        yuvInfo.pData = nullptr;
    }
}

// 使用示例
inline void TestAlphaMergeYuvs() 
{
    try {
        constexpr int width = YUV_W;
        constexpr int height = YUV_H;
        constexpr int stride = YUV_W;
        constexpr double alphaRatio = 0.7;
        auto s1 = CROP_COVER_DIR + "/crop1.yuv";
        auto s2 = CROP_COVER_DIR + "/crop20.yuv";

        // 从文件加载YUV数据到内存
        YuvInfo yuv1 = LoadYuvFromFile(s1, width, height, stride);
        YuvInfo yuv2 = LoadYuvFromFile(s2, width, height, stride);

        // 使用内存版本进行混合
        AlphaMergeYuvs(yuv1, yuv2, alphaRatio, "alphaMerge1.yuv");

        // 使用OpenCV内存版本进行混合
        AlphaMergeYuvsOpenCV(yuv1, yuv2, alphaRatio, "alphaMerge2.yuv");

        // 释放内存
        FreeYuvInfo(yuv1);
        FreeYuvInfo(yuv2);

        auto out3 = LoadYuvFromFile("alphaMerge2.yuv", width, height, stride);
        DumpYuv2Jpg(out3, "alphaMerge2.jpg");

        std::cout << "YUV memory data merged successfully!" << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return;
    }

    return;
}

#endif // !ALPHAMERGEYUV_H
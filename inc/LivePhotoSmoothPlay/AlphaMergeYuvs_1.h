    }
}

// 3. 将YUV数据转换为BGR Mat（方便进一步处理）
inline cv::Mat Yuv2BgrMat(const YuvInfo& in) {
    // 参数验证
    if (!in.pData) {
        throw std::invalid_argument("YUV data pointer cannot be null");
    }

    if (in.width <= 0 || in.height <= 0 || in.stride <= 0) {
        throw std::invalid_argument("Invalid YUV dimensions");
    }

    if (in.stride < in.width) {
        throw std::invalid_argument("stride must be >= width");
    }

    int w = in.width;
    int h = in.height;
    int stride = in.stride;

    // 创建连续的YUV数据（无padding）
    cv::Mat yuvContinuous;

    if (stride == w) {
        // 如果stride等于width，直接使用
        yuvContinuous = cv::Mat(h + h / 2, w, CV_8UC1, in.pData);
    }
    else {
        // 否则需要去除padding
        yuvContinuous = cv::Mat(h + h / 2, w, CV_8UC1);

        size_t ySize = static_cast<size_t>(stride) * h;
        size_t uvSize = static_cast<size_t>(stride / 2) * (h / 2);
        int uvWidth = w / 2;
        int uvStride = stride / 2;

        // 复制Y平面
        for (int row = 0; row < h; ++row) {
            const unsigned char* srcRow = in.pData + row * stride;
            unsigned char* dstRow = yuvContinuous.ptr(row);
            std::copy(srcRow, srcRow + w, dstRow);
        }

        // 复制U平面
        size_t uStart = ySize;
        for (int row = 0; row < h / 2; ++row) {
            const unsigned char* srcRow = in.pData + uStart + row * uvStride;
            unsigned char* dstRow = yuvContinuous.ptr(h + row);
            std::copy(srcRow, srcRow + uvWidth, dstRow);
        }

        // 复制V平面
        size_t vStart = ySize + uvSize;
        for (int row = 0; row < h / 2; ++row) {
            const unsigned char* srcRow = in.pData + vStart + row * uvStride;
            unsigned char* dstRow = yuvContinuous.ptr(h + h / 2 + row);
            std::copy(srcRow, srcRow + uvWidth, dstRow);
        }
    }

    // 转换为BGR
    cv::Mat bgrMat;
    cv::cvtColor(yuvContinuous, bgrMat, cv::COLOR_YUV2BGR_I420);

    return bgrMat;
}

// 4. 使用OpenCV的高效方法（适用于stride > width的情况）
inline void DumpYuv2JpgOptimized(const YuvInfo& in, const std::string& out) {
    // 参数验证
    if (!in.pData) {
        throw std::invalid_argument("YUV data pointer cannot be null");
    }

    if (in.width <= 0 || in.height <= 0 || in.stride <= 0) {
        throw std::invalid_argument("Invalid YUV dimensions");
    }

    if (in.stride < in.width) {
        throw std::invalid_argument("stride must be >= width");
    }

    int w = in.width;
    int h = in.height;
    int stride = in.stride;

    // 方法：使用OpenCV的步长感知转换
    // 创建Y、U、V平面的Mat视图
    cv::Mat yPlane(h, stride, CV_8UC1, in.pData);
    cv::Mat uvPlane(h / 2, stride / 2, CV_8UC2, in.pData + stride * h);

    // 裁剪出有效区域（去除padding）
    cv::Rect yRoi(0, 0, w, h);
    cv::Rect uvRoi(0, 0, w / 2, h / 2);

    cv::Mat yValid = yPlane(yRoi).clone();  // 需要克隆，因为原始数据有padding
    cv::Mat uvValid = uvPlane(uvRoi).clone();

    // 合并YUV平面
    cv::Mat yuv420;
    std::vector<cv::Mat> yuvPlanes;

    // 分离UV平面的U和V
    cv::Mat uvChannels[2];
    cv::split(uvValid, uvChannels);

    yuvPlanes.push_back(yValid);
    yuvPlanes.push_back(uvChannels[0]);  // U平面
    yuvPlanes.push_back(uvChannels[1]);  // V平面

    cv::vconcat(yuvPlanes, yuv420);

    // 转换为BGR
    cv::Mat bgrMat;
    cv::cvtColor(yuv420, bgrMat, cv::COLOR_YUV2BGR_I420);

    // 保存为高质量JPEG
    std::vector<int> compression_params = {
        cv::IMWRITE_JPEG_QUALITY, 95,
        cv::IMWRITE_JPEG_PROGRESSIVE, 1,
        cv::IMWRITE_JPEG_OPTIMIZE, 1,
        cv::IMWRITE_JPEG_CHROMA_QUALITY, 95  // 色度质量
    };

    if (!cv::imwrite(out, bgrMat, compression_params)) {
        throw std::runtime_error("Failed to save JPEG image: " + out);
    }
}

// 纯C++实现，从内存YUV数据混合
inline void AlphaMergeYuvs(const YuvInfo& yuv1, const YuvInfo& yuv2,double yuv1AlphaRatio, std::string_view outYuv) {

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


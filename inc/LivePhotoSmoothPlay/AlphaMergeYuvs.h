#ifndef ALPHAMERGEYUV_H
#define ALPHAMERGEYUV_H

#include "OpenCVCommon.h"
#include "FFmpegDefine.h"


// YUV内存数据结构
struct YuvInfo {
    unsigned char* pData; // yuv内存首地址，使用unsigned char更合适
    int width;            // 宽
    int height;           // 高
    int stride;           // stride
};

// 1. 将YUV数据直接保存到文件
inline void DumpYuv(const YuvInfo& in, const std::string& out) {
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

    // 计算YUV420文件大小
    size_t ySize = static_cast<size_t>(in.stride) * in.height;
    size_t uvSize = static_cast<size_t>(in.stride / 2) * (in.height / 2);
    size_t totalSize = ySize + uvSize * 2;  // Y + U + V

    // 打开输出文件
    std::ofstream outFile(out, std::ios::binary);
    if (!outFile.is_open()) {
        throw std::runtime_error("Cannot create output file: " + out);
    }

    // 直接写入YUV数据
    outFile.write(reinterpret_cast<const char*>(in.pData), totalSize);

    // 检查是否写入成功
    if (!outFile.good()) {
        throw std::runtime_error("Failed to write YUV data to file: " + out);
    }

    outFile.close();
}

// 2. 将YUV数据转换为高质量JPEG并保存
inline void DumpYuv2Jpg(const YuvInfo& in, const std::string& out) {
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

    // 计算YUV数据大小
    int w = in.width;
    int h = in.height;
    int stride = in.stride;

    size_t ySize = static_cast<size_t>(stride) * h;
    size_t uvSize = static_cast<size_t>(stride / 2) * (h / 2);

    // 创建OpenCV矩阵表示YUV数据（I420格式）
    cv::Mat yuvMat(h + h / 2, stride, CV_8UC1, in.pData);

    // 方法1：使用cvtColor直接转换（需要去除padding）
    if (stride == w) {
        // 如果stride等于width，数据是连续的，可以直接转换
        cv::Mat bgrMat;

        // 将YUV420转换为BGR
        cv::cvtColor(yuvMat, bgrMat, cv::COLOR_YUV2BGR_I420);

        // 保存为高质量JPEG
        std::vector<int> compression_params;
        compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
        compression_params.push_back(95);  // 高质量，0-100，95是很好的质量
        compression_params.push_back(cv::IMWRITE_JPEG_PROGRESSIVE);
        compression_params.push_back(1);  // 启用渐进式JPEG
        compression_params.push_back(cv::IMWRITE_JPEG_OPTIMIZE);
        compression_params.push_back(1);  // 启用优化

        if (!cv::imwrite(out, bgrMat, compression_params)) {
            throw std::runtime_error("Failed to save JPEG image: " + out);
        }
        else {
            PV("succeed to save jpg image to", out);
        }
    }
    else {
        // 方法2：如果stride > width，需要去除padding
        // 创建一个连续的YUV数据（无padding）
        cv::Mat yuvContinuous(h + h / 2, w, CV_8UC1);

        // 复制Y平面（去除padding）
        for (int row = 0; row < h; ++row) {
            const unsigned char* srcRow = in.pData + row * stride;
            unsigned char* dstRow = yuvContinuous.ptr(row);
            std::copy(srcRow, srcRow + w, dstRow);
        }

        // 复制U平面（去除padding）
        size_t uStart = ySize;
        int uvWidth = w / 2;
        int uvStride = stride / 2;

        for (int row = 0; row < h / 2; ++row) {
            const unsigned char* srcRow = in.pData + uStart + row * uvStride;
            unsigned char* dstRow = yuvContinuous.ptr(h + row);
            std::copy(srcRow, srcRow + uvWidth, dstRow);
        }

        // 复制V平面（去除padding）
        size_t vStart = ySize + uvSize;

        for (int row = 0; row < h / 2; ++row) {
            const unsigned char* srcRow = in.pData + vStart + row * uvStride;
            unsigned char* dstRow = yuvContinuous.ptr(h + h / 2 + row);
            std::copy(srcRow, srcRow + uvWidth, dstRow);
        }

        // 将YUV420转换为BGR
        cv::Mat bgrMat;
        cv::cvtColor(yuvContinuous, bgrMat, cv::COLOR_YUV2BGR_I420);

        // 保存为高质量JPEG
        std::vector<int> compression_params;
        compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
        compression_params.push_back(95);  // 高质量
        compression_params.push_back(cv::IMWRITE_JPEG_PROGRESSIVE);
        compression_params.push_back(1);    // 渐进式
        compression_params.push_back(cv::IMWRITE_JPEG_OPTIMIZE);
        compression_params.push_back(1);    // 优化

        if (!cv::imwrite(out, bgrMat, compression_params)) {
            throw std::runtime_error("Failed to save JPEG image: " + out);
        }
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
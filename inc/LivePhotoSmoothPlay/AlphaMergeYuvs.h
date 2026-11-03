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

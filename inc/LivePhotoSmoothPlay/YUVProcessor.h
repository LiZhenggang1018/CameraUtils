/*
 * Copyright (c) 2026 by Vivo Technologies Co., Ltd, All Rights Reserved.
 * @Description:
 * @Author: lizhenggang
 * @Date: 2026-02-02 14:59:59
 * @LastEditTime: 2026-02-09 09:50:25
 * @FilePath: \OpenCV101\YUVProcessor.h
 */

#ifndef INC_LIVEPHOTOSMOOTHPLAY_YUVPROCESSOR_H
#define INC_LIVEPHOTOSMOOTHPLAY_YUVPROCESSOR_H

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

class YUVProcessor {
private:
    int width_;
    int height_;
    std::vector<uint8_t> yuvData_;

public:
    YUVProcessor(int width, int height)
        : width_(width),
          height_(height)
    {}

    // 读取YUV420文件
    bool ReadYuvFile(const std::string& filepath)
    {
        // YUV420文件大小：Y分量 + U分量 + V分量
        size_t fileSize = width_ * height_ * 3 / 2;
        yuvData_.resize(fileSize);

        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            std::cerr << "无法打开文件: " << filepath << std::endl;
            return false;
        }

        file.read(reinterpret_cast<char*>(yuvData_.data()), fileSize);
        if (!file) {
            std::cerr << "读取文件失败: " << filepath << std::endl;
            return false;
        }

        return true;
    }

    // 裁剪并生成新YUV文件
    bool CropAndResize(float cropPercent, const std::string& outputPath)
    {
        if (cropPercent <= 0 || cropPercent >= 100) {
            std::cerr << "裁剪比例必须在0-100之间" << std::endl;
            return false;
        }

        // 计算裁剪像素数（每边）
        int cropX = static_cast<int>(width_ * cropPercent / 100.0f);
        int cropY = static_cast<int>(height_ * cropPercent / 100.0f);

        // 确保裁剪后尺寸是偶数（YUV420要求）
        cropX = (cropX / 2) * 2;
        cropY = (cropY / 2) * 2;

        // 计算裁剪后的区域
        int cropWidth = width_ - 2 * cropX;
        int cropHeight = height_ - 2 * cropY;

        if (cropWidth <= 0 || cropHeight <= 0) {
            std::cerr << "裁剪后尺寸太小: " << cropWidth << "x" << cropHeight << std::endl;
            return false;
        }

        // 创建输出缓冲区
        std::vector<uint8_t> outputData(width_ * height_ * 3 / 2);

        // 处理Y分量
        ProcessPlane(outputData.data(), 0, width_, height_, cropX, cropY, cropWidth, cropHeight, 1, 1);

        // 处理U分量（YUV420，UV分量尺寸减半）
        ProcessPlane(outputData.data() + width_ * height_, width_ * height_, width_ / 2, height_ / 2, cropX / 2,
                     cropY / 2, cropWidth / 2, cropHeight / 2, 1, 1);

        // 处理V分量
        ProcessPlane(outputData.data() + width_ * height_ * 5 / 4, width_ * height_ * 5 / 4, width_ / 2, height_ / 2,
                     cropX / 2, cropY / 2, cropWidth / 2, cropHeight / 2, 1, 1);

        // 写入文件
        return WriteYuvFile(outputPath, outputData);
    }

private:
    // 处理单个平面（Y、U或V）
    void ProcessPlane(uint8_t* output, size_t inputOffset, int planeWidth, int planeHeight, int cropX, int cropY,
                      int cropWidth, int cropHeight, int scaleX, int scaleY)
    {

        // 计算裁剪区域的起始位置
        const uint8_t* inputPlane = yuvData_.data() + inputOffset;

        // 填充输出平面
        for (int y = 0; y < planeHeight; y++) {
            for (int x = 0; x < planeWidth; x++) {
                // 计算在裁剪图像中的对应位置
                float srcX = cropX + (static_cast<float>(x) * cropWidth / planeWidth);
                float srcY = cropY + (static_cast<float>(y) * cropHeight / planeHeight);

                int x1 = static_cast<int>(srcX);
                int y1 = static_cast<int>(srcY);
                int x2 = std::min(x1 + 1, cropX + cropWidth - 1);
                int y2 = std::min(y1 + 1, cropY + cropHeight - 1);

                // 边界检查（修复 #9：x/y 分开用各自的边界）
                x1 = std::max(0, std::min(x1, cropX + cropWidth - 1));
                y1 = std::max(0, std::min(y1, cropY + cropHeight - 1));
                x2 = std::max(0, std::min(x2, cropX + cropWidth - 1));
                y2 = std::max(0, std::min(y2, cropY + cropHeight - 1));

                // 获取四个相邻像素
                float fx = srcX - x1;
                float fy = srcY - y1;

                uint8_t v11 = inputPlane[y1 * planeWidth + x1];
                uint8_t v12 = inputPlane[y1 * planeWidth + x2];
                uint8_t v21 = inputPlane[y2 * planeWidth + x1];
                uint8_t v22 = inputPlane[y2 * planeWidth + x2];

                // 双线性插值
                float interpolated =
                    v11 * (1 - fx) * (1 - fy) + v12 * fx * (1 - fy) + v21 * (1 - fx) * fy + v22 * fx * fy;

                // 写入输出
                output[y * planeWidth + x] = static_cast<uint8_t>(std::clamp(interpolated, 0.0f, 255.0f));
            }
        }
    }

    // 写入YUV文件
    bool WriteYuvFile(const std::string& filepath, const std::vector<uint8_t>& data)
    {
        std::ofstream file(filepath, std::ios::binary);
        if (!file) {
            std::cerr << "无法创建输出文件: " << filepath << std::endl;
            return false;
        }

        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        if (!file) {
            std::cerr << "写入文件失败: " << filepath << std::endl;
            return false;
        }

        return true;
    }
};

#endif // INC_LIVEPHOTOSMOOTHPLAY_YUVPROCESSOR_H
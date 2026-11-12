/*
 * Copyright (c) 2026 by Vivo Technologies Co., Ltd, All Rights Reserved.
 * @Description:
 * @Author: lizhenggang
 * @Date: 2026-02-02 14:59:59
 * @LastEditTime: 2026-02-09 09:50:25
 * @FilePath: \OpenCV101\YUVProcessor.h
 */

#ifndef YUVPROCESSOR_H_
#define YUVPROCESSOR_H_

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <memory>

class YUVProcessor
{
private:
    int width_;
    int height_;
    std::vector<uint8_t> yuv_data_;

public:
    YUVProcessor(int width, int height) : width_(width), height_(height) {}

    // 读取YUV420文件
    bool readYUVFile(const std::string &filepath)
    {
        // YUV420文件大小：Y分量 + U分量 + V分量
        size_t file_size = width_ * height_ * 3 / 2;
        yuv_data_.resize(file_size);

        std::ifstream file(filepath, std::ios::binary);
        if (!file)
        {
            std::cerr << "无法打开文件: " << filepath << std::endl;
            return false;
        }

        file.read(reinterpret_cast<char *>(yuv_data_.data()), file_size);
        if (!file)
        {
            std::cerr << "读取文件失败: " << filepath << std::endl;
            return false;
        }

        return true;
    }

    // 裁剪并生成新YUV文件
    bool cropAndResize(float crop_percent, const std::string &output_path)
    {
        if (crop_percent <= 0 || crop_percent >= 100)
        {
            std::cerr << "裁剪比例必须在0-100之间" << std::endl;
            return false;
        }

        // 计算裁剪像素数（每边）
        int crop_x = static_cast<int>(width_ * crop_percent / 100.0f);
        int crop_y = static_cast<int>(height_ * crop_percent / 100.0f);

        // 确保裁剪后尺寸是偶数（YUV420要求）
        crop_x = (crop_x / 2) * 2;
        crop_y = (crop_y / 2) * 2;

        // 计算裁剪后的区域
        int crop_width = width_ - 2 * crop_x;
        int crop_height = height_ - 2 * crop_y;

        if (crop_width <= 0 || crop_height <= 0)
        {
            std::cerr << "裁剪后尺寸太小: " << crop_width << "x" << crop_height << std::endl;
            return false;
        }

        // 创建输出缓冲区
        std::vector<uint8_t> output_data(width_ * height_ * 3 / 2);

        // 处理Y分量
        processPlane(output_data.data(), 0, width_, height_, crop_x, crop_y, crop_width, crop_height, 1, 1);

        // 处理U分量（YUV420，UV分量尺寸减半）
        processPlane(output_data.data() + width_ * height_, width_ * height_, width_ / 2, height_ / 2,
                     crop_x / 2, crop_y / 2, crop_width / 2, crop_height / 2, 1, 1);

        // 处理V分量
        processPlane(output_data.data() + width_ * height_ * 5 / 4, width_ * height_ * 5 / 4, width_ / 2, height_ / 2,
                     crop_x / 2, crop_y / 2, crop_width / 2, crop_height / 2, 1, 1);

        // 写入文件
        return writeYUVFile(output_path, output_data);
    }

private:
    // 处理单个平面（Y、U或V）
    void processPlane(uint8_t *output, size_t input_offset, int plane_width, int plane_height, int crop_x, int crop_y,
                      int crop_width, int crop_height, int scale_x, int scale_y)
    {

        // 计算裁剪区域的起始位置
        const uint8_t *input_plane = yuv_data_.data() + input_offset;

        // 填充输出平面
        for (int y = 0; y < plane_height; y++)
        {
            for (int x = 0; x < plane_width; x++)
            {
                // 计算在裁剪图像中的对应位置
                float src_x = crop_x + (static_cast<float>(x) * crop_width / plane_width);
                float src_y = crop_y + (static_cast<float>(y) * crop_height / plane_height);

                int x1 = static_cast<int>(src_x);
                int y1 = static_cast<int>(src_y);
                int x2 = std::min(x1 + 1, crop_x + crop_width - 1);
                int y2 = std::min(y1 + 1, crop_y + crop_height - 1);

                // 边界检查（修复 #9：x/y 分开用各自的边界）
                x1 = std::max(0, std::min(x1, crop_x + crop_width  - 1));
                y1 = std::max(0, std::min(y1, crop_y + crop_height - 1));
                x2 = std::max(0, std::min(x2, crop_x + crop_width  - 1));
                y2 = std::max(0, std::min(y2, crop_y + crop_height - 1));

                // 获取四个相邻像素
                float fx = src_x - x1;
                float fy = src_y - y1;

                uint8_t v11 = input_plane[y1 * plane_width + x1];
                uint8_t v12 = input_plane[y1 * plane_width + x2];
                uint8_t v21 = input_plane[y2 * plane_width + x1];
                uint8_t v22 = input_plane[y2 * plane_width + x2];

                // 双线性插值
                float interpolated =
                    v11 * (1 - fx) * (1 - fy) + v12 * fx * (1 - fy) + v21 * (1 - fx) * fy + v22 * fx * fy;

                // 写入输出
                output[y * plane_width + x] = static_cast<uint8_t>(std::clamp(interpolated, 0.0f, 255.0f));
            }
        }
    }

    // 写入YUV文件
    bool writeYUVFile(const std::string &filepath, const std::vector<uint8_t> &data)
    {
        std::ofstream file(filepath, std::ios::binary);
        if (!file)
        {
            std::cerr << "无法创建输出文件: " << filepath << std::endl;
            return false;
        }

        file.write(reinterpret_cast<const char *>(data.data()), data.size());
        if (!file)
        {
            std::cerr << "写入文件失败: " << filepath << std::endl;
            return false;
        }

        return true;
    }
};

#endif // YUVPROCESSOR_H_
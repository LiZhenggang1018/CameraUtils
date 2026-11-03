/*
 * Copyright (c) 2026 by Vivo Technologies Co., Ltd, All Rights Reserved.
 * @Description: read yuv and format convert utils
 * @Author: lizhenggang
 * @Date: 2026-02-04 10:47:42
 * @LastEditTime: 2026-02-09 09:49:23
 * @FilePath: \OpenCV101\YUVImageReader.h
 */

#ifndef YUVIMAGEREADER_H_
#define YUVIMAGEREADER_H_

#include "OpenCVCommon.h"

class YUVImageReader
{
public:
    YUVImageReader(int width, int height);
    ~YUVImageReader() = default;

    /**
     * @brief 读取YUV420P文件并转换为BGR格式
     * @param filepath YUV文件路径
     * @return 转换后的BGR图像
     */
    cv::Mat readYUV420P(const std::string &filepath);

    /**
     * @brief 读取YUV420P文件并保持YUV格式（三个通道分离）
     * @param filepath YUV文件路径
     * @return 包含Y、U、V三个通道的向量
     */
    std::vector<cv::Mat> readYUV420PChannels(const std::string &filepath);

    /**
     * @brief 将BGR图像保存为YUV420P文件
     * @param image BGR图像
     * @param filepath 输出文件路径
     */
    void saveAsYUV420P(const cv::Mat &image, const std::string &filepath);

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    int width_;
    int height_;
    int y_size_;
    int uv_size_;

    void convertBGRtoYUV420P(const cv::Mat &bgr_image, std::vector<uint8_t> &yuv_data) const;
};

#endif // YUVIMAGEREADER_H_
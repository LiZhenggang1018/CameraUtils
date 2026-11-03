/*
 * @Description: Simple YUV Image Reader Source File
 */

#include "YUVImageReader.h"

YUVImageReader::YUVImageReader(int width, int height) : width_(width), height_(height)
{
    y_size_ = width * height;
    uv_size_ = (width / 2) * (height / 2);
}

cv::Mat YUVImageReader::readYUV420P(const std::string &filepath)
{
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        throw std::runtime_error("无法打开文件: " + filepath);
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // 验证文件大小
    size_t expected_size = static_cast<size_t>(y_size_ + 2 * uv_size_);
    if (static_cast<size_t>(file_size) != expected_size)
    {
        throw std::runtime_error("文件大小与YUV420P格式不匹配");
    }

    // 读取YUV数据
    std::vector<uint8_t> yuv_data(expected_size);
    if (!file.read(reinterpret_cast<char *>(yuv_data.data()), expected_size))
    {
        throw std::runtime_error("读取文件失败");
    }

    // 创建Y、U、V平面
    cv::Mat y_plane(height_, width_, CV_8UC1, yuv_data.data());

    // U平面（降采样）
    cv::Mat u_plane_subsampled(height_ / 2, width_ / 2, CV_8UC1, yuv_data.data() + y_size_);

    // V平面（降采样）
    cv::Mat v_plane_subsampled(height_ / 2, width_ / 2, CV_8UC1, yuv_data.data() + y_size_ + uv_size_);

    // 将U、V平面上采样到原始尺寸
    cv::Mat u_plane, v_plane;
    cv::resize(u_plane_subsampled, u_plane, cv::Size(width_, height_), 0, 0, cv::INTER_LINEAR);
    cv::resize(v_plane_subsampled, v_plane, cv::Size(width_, height_), 0, 0, cv::INTER_LINEAR);

    // 合并YUV通道
    std::vector<cv::Mat> yuv_channels = {y_plane, u_plane, v_plane};
    cv::Mat yuv_image;
    cv::merge(yuv_channels, yuv_image);

    // 转换为BGR
    cv::Mat bgr_image;
    cv::cvtColor(yuv_image, bgr_image, cv::COLOR_YUV2BGR);

    return bgr_image;
}

std::vector<cv::Mat> YUVImageReader::readYUV420PChannels(const std::string &filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("无法打开文件: " + filepath);
    }

    size_t total_size = static_cast<size_t>(y_size_ + 2 * uv_size_);
    std::vector<uint8_t> yuv_data(total_size);
    file.read(reinterpret_cast<char *>(yuv_data.data()), total_size);

    // 创建Y、U、V平面
    cv::Mat y_plane(height_, width_, CV_8UC1, yuv_data.data());
    cv::Mat u_plane_subsampled(height_ / 2, width_ / 2, CV_8UC1, yuv_data.data() + y_size_);
    cv::Mat v_plane_subsampled(height_ / 2, width_ / 2, CV_8UC1, yuv_data.data() + y_size_ + uv_size_);

    return {y_plane, u_plane_subsampled, v_plane_subsampled};
}

void YUVImageReader::convertBGRtoYUV420P(const cv::Mat &bgr_image, std::vector<uint8_t> &yuv_data) const
{
    // 转换为YUV
    cv::Mat yuv_image;
    cv::cvtColor(bgr_image, yuv_image, cv::COLOR_BGR2YUV);

    // 分离通道
    std::vector<cv::Mat> channels;
    cv::split(yuv_image, channels);

    // 对U和V通道进行下采样
    cv::Mat u_downsampled, v_downsampled;
    cv::resize(channels[1], u_downsampled, cv::Size(width_ / 2, height_ / 2), 0, 0, cv::INTER_AREA);
    cv::resize(channels[2], v_downsampled, cv::Size(width_ / 2, height_ / 2), 0, 0, cv::INTER_AREA);

    // 分配内存
    yuv_data.resize(y_size_ + 2 * uv_size_);

    // 复制Y平面
    std::memcpy(yuv_data.data(), channels[0].data, y_size_);

    // 复制U平面
    std::memcpy(yuv_data.data() + y_size_, u_downsampled.data, uv_size_);

    // 复制V平面
    std::memcpy(yuv_data.data() + y_size_ + uv_size_, v_downsampled.data, uv_size_);
}

void YUVImageReader::saveAsYUV420P(const cv::Mat &image,
                                   const std::string &filepath)
{
    if (image.empty())
    {
        throw std::runtime_error("输入图像为空");
    }

    // 确保图像尺寸正确
    cv::Mat resized_image;
    if (image.cols != width_ || image.rows != height_)
    {
        cv::resize(image, resized_image, cv::Size(width_, height_));
    }
    else
    {
        resized_image = image.clone();
    }

    // 转换为YUV420P
    std::vector<uint8_t> yuv_data;
    convertBGRtoYUV420P(resized_image, yuv_data);

    // 保存到文件
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("无法创建输出文件: " + filepath);
    }

    file.write(reinterpret_cast<const char *>(yuv_data.data()), yuv_data.size());
}
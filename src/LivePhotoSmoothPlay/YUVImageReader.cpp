/*
 * @Description: YUV420P <-> BGR reader/writer
 */

#include "YUVImageReader.h"

YUVImageReader::YUVImageReader(int width, int height)
    : width_(width), height_(height)
{
    y_size_  = width * height;
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

    const size_t expected_size = static_cast<size_t>(y_size_ + 2 * uv_size_);
    if (static_cast<size_t>(file_size) != expected_size)
    {
        throw std::runtime_error("文件大小与YUV420P格式不匹配");
    }

    std::vector<uint8_t> yuv_data(expected_size);
    if (!file.read(reinterpret_cast<char *>(yuv_data.data()),
                   static_cast<std::streamsize>(expected_size)))
    {
        throw std::runtime_error("读取文件失败");
    }

    // 修复 #3：用 COLOR_YUV2BGR_I420 直接转，一次完成 limited-range 4:2:0 → BGR
    // 此前用 resize + merge + COLOR_YUV2BGR 会走 full-range 路径，与 FFmpeg limited-range 不匹配
    cv::Mat yuv420(height_ * 3 / 2, width_, CV_8UC1, yuv_data.data());
    cv::Mat bgr_image;
    cv::cvtColor(yuv420, bgr_image, cv::COLOR_YUV2BGR_I420);

    return bgr_image;
}

std::vector<cv::Mat> YUVImageReader::readYUV420PChannels(const std::string &filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("无法打开文件: " + filepath);
    }

    const size_t total_size = static_cast<size_t>(y_size_ + 2 * uv_size_);
    std::vector<uint8_t> yuv_data(total_size);
    if (!file.read(reinterpret_cast<char *>(yuv_data.data()),
                   static_cast<std::streamsize>(total_size)))
    {
        throw std::runtime_error("读取文件失败");
    }

    // 修复 #1：用 clone() 深拷贝，避免返回的 cv::Mat 引用局部 vector
    cv::Mat y_plane = cv::Mat(height_, width_, CV_8UC1, yuv_data.data()).clone();
    cv::Mat u_plane = cv::Mat(height_ / 2, width_ / 2, CV_8UC1,
                              yuv_data.data() + y_size_).clone();
    cv::Mat v_plane = cv::Mat(height_ / 2, width_ / 2, CV_8UC1,
                              yuv_data.data() + y_size_ + uv_size_).clone();

    return {y_plane, u_plane, v_plane};
}

void YUVImageReader::convertBGRtoYUV420P(const cv::Mat &bgr_image,
                                          std::vector<uint8_t> &yuv_data) const
{
    // 修复 #7：显式检查连续性，避免 ROI 视图读错
    CV_Assert(!bgr_image.empty());
    CV_Assert(bgr_image.type() == CV_8UC3);
    CV_Assert(bgr_image.isContinuous());

    // 修复 #3：用 COLOR_BGR2YUV_I420，一次性输出 limited-range I420
    // 与 readYUV420P 的 COLOR_YUV2BGR_I420 自洽
    cv::Mat yuv420;
    cv::cvtColor(bgr_image, yuv420, cv::COLOR_BGR2YUV_I420);

    yuv_data.resize(y_size_ + 2 * uv_size_);
    std::memcpy(yuv_data.data(), yuv420.data, yuv_data.size());
}

void YUVImageReader::saveAsYUV420P(const cv::Mat &image,
                                   const std::string &filepath)
{
    if (image.empty())
    {
        throw std::runtime_error("输入图像为空");
    }
    if (image.type() != CV_8UC3)
    {
        throw std::runtime_error("输入图像必须是 CV_8UC3 (BGR)");
    }

    cv::Mat resized_image;
    if (image.cols != width_ || image.rows != height_)
    {
        // 缩小时用 INTER_AREA，放大时用 INTER_CUBIC
        const int interp = (image.cols > width_ || image.rows > height_)
                               ? cv::INTER_AREA
                               : cv::INTER_CUBIC;
        cv::resize(image, resized_image, cv::Size(width_, height_), 0, 0, interp);
    }
    else
    {
        // 关键：克隆，保证传入 convertBGRtoYUV420P 的是连续内存
        resized_image = image.clone();
    }

    std::vector<uint8_t> yuv_data;
    convertBGRtoYUV420P(resized_image, yuv_data);

    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("无法创建输出文件: " + filepath);
    }
    file.write(reinterpret_cast<const char *>(yuv_data.data()),
               static_cast<std::streamsize>(yuv_data.size()));
}
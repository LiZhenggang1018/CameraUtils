/*
 * @Description: YUV420P <-> BGR reader/writer
 */

#include "YUVImageReader.h"

YUVImageReader::YUVImageReader(int width, int height)
    : width_(width),
      height_(height),
      ySize_(width * height),
      uvSize_((width / 2) * (height / 2))
{}

cv::Mat YUVImageReader::readYUV420P(const std::string& filepath) const
{
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + filepath);
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    const auto expectedSize = static_cast<size_t>(ySize_) + (2 * static_cast<size_t>(uvSize_));
    if (static_cast<size_t>(fileSize) != expectedSize) {
        throw std::runtime_error("文件大小与YUV420P格式不匹配");
    }

    std::vector<uint8_t> yuvData(expectedSize);
    if (!file.read(reinterpret_cast<char*>(yuvData.data()), static_cast<std::streamsize>(expectedSize))) {
        throw std::runtime_error("读取文件失败");
    }

    // 修复 #3：用 COLOR_YUV2BGR_I420 直接转，一次完成 limited-range 4:2:0 → BGR
    // 此前用 resize + merge + COLOR_YUV2BGR 会走 full-range 路径，与 FFmpeg limited-range 不匹配
    cv::Mat yuv420(height_ * 3 / 2, width_, CV_8UC1, yuvData.data());
    cv::Mat bgrImage;
    cv::cvtColor(yuv420, bgrImage, cv::COLOR_YUV2BGR_I420);

    return bgrImage;
}

std::vector<cv::Mat> YUVImageReader::readYUV420PChannels(const std::string& filepath) const
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + filepath);
    }

    const auto totalSize = static_cast<size_t>(ySize_) + (2 * static_cast<size_t>(uvSize_));
    std::vector<uint8_t> yuvData(totalSize);
    if (!file.read(reinterpret_cast<char*>(yuvData.data()), static_cast<std::streamsize>(totalSize))) {
        throw std::runtime_error("读取文件失败");
    }

    // 修复 #1：用 clone() 深拷贝，避免返回的 cv::Mat 引用局部 vector
    cv::Mat yPlane = cv::Mat(height_, width_, CV_8UC1, yuvData.data()).clone();
    cv::Mat uPlane = cv::Mat(height_ / 2, width_ / 2, CV_8UC1, yuvData.data() + ySize_).clone();
    cv::Mat vPlane = cv::Mat(height_ / 2, width_ / 2, CV_8UC1, yuvData.data() + ySize_ + uvSize_).clone();

    return {yPlane, uPlane, vPlane};
}

void YUVImageReader::convertBGRtoYUV420P(const cv::Mat& bgrImage, std::vector<uint8_t>& yuvData) const
{
    // 修复 #7：显式检查连续性，避免 ROI 视图读错
    CV_Assert(!bgrImage.empty());
    CV_Assert(bgrImage.type() == CV_8UC3);
    CV_Assert(bgrImage.isContinuous());

    // 修复 #3：用 COLOR_BGR2YUV_I420，一次性输出 limited-range I420
    // 与 readYUV420P 的 COLOR_YUV2BGR_I420 自洽
    cv::Mat yuv420;
    cv::cvtColor(bgrImage, yuv420, cv::COLOR_BGR2YUV_I420);

    yuvData.resize(static_cast<size_t>(ySize_) + (2 * static_cast<size_t>(uvSize_)));
    std::memcpy(yuvData.data(), yuv420.data, yuvData.size());
}

void YUVImageReader::saveAsYUV420P(const cv::Mat& image, const std::string& filepath)
{
    if (image.empty()) {
        throw std::runtime_error("输入图像为空");
    }
    if (image.type() != CV_8UC3) {
        throw std::runtime_error("输入图像必须是 CV_8UC3 (BGR)");
    }

    cv::Mat resizedImage;
    if (image.cols != width_ || image.rows != height_) {
        // 缩小时用 INTER_AREA，放大时用 INTER_CUBIC
        const int interp = (image.cols > width_ || image.rows > height_) ? cv::INTER_AREA : cv::INTER_CUBIC;
        cv::resize(image, resizedImage, cv::Size(width_, height_), 0, 0, interp);
    } else {
        // 关键：克隆，保证传入 convertBGRtoYUV420P 的是连续内存
        resizedImage = image.clone();
    }

    std::vector<uint8_t> yuvData;
    convertBGRtoYUV420P(resizedImage, yuvData);

    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("无法创建输出文件: " + filepath);
    }
    file.write(reinterpret_cast<const char*>(yuvData.data()), static_cast<std::streamsize>(yuvData.size()));
}
#ifndef INC_IMAGEREG_HARRISDETECT_H_
#define INC_IMAGEREG_HARRISDETECT_H_

#include "OpenCVCommon.h"
#include "CommonEnv.h"
#include <iostream>

// 路径常量：改成"函数内 static"，避免跨文件静态初始化顺序问题
inline const std::string& ImageRegDataPathImpl() {
    static const std::string s = std::string(PROJECT_ROOT_DIR) + "/data/ImageReg/";
    return s;
}
inline const std::string& ImageRegInFileImpl() {
    static const std::string s = ImageRegDataPathImpl() + "lhc.jpg";
    return s;
}

// 辅助函数：如果图像尺寸超过 maxSize，则等比例缩放并返回，否则返回原图
inline cv::Mat fitToScreen(const cv::Mat& img, int maxSize = 800) {
    int h = img.rows, w = img.cols;
    if (h <= maxSize && w <= maxSize)
        return img;
    double scale = std::min((double)maxSize / w, (double)maxSize / h);
    cv::Mat resized;
    cv::resize(img, resized, cv::Size(), scale, scale, cv::INTER_LINEAR);
    return resized;
}

inline void HarrisTest() {
    cv::Mat gray = cv::imread(ImageRegInFileImpl(), cv::IMREAD_GRAYSCALE);
    if (gray.empty()) return;

    cv::Mat eigen;
    // cornerEigenValsAndVecs计算图像每个像素邻域内 结构张量（structure tensor） 的特征值与特征向量
    //void cv::cornerEigenValsAndVecs(
    //    InputArray src, // 输入单通道图像（CV_8U 或 CV_32F）。
    //    OutputArray dst, // 输出的 6 通道浮点图像，每个像素存储特征值与特征向量。
    //    int blockSize, // 邻域求和窗口大小（类比 Harris 中的窗口）。只取奇数，例如 3、5。
    //    int ksize, // Sobel 算子的孔径大小，用于计算图像一阶导数。必须为奇数，如 3、5、7。
    //    int borderType = BORDER_DEFAULT // 边界填充方式
    //);
    cv::cornerEigenValsAndVecs(gray, eigen, 3, 3);

    std::vector<cv::Mat> channels(6);
    cv::split(eigen, channels);
    cv::Mat lambda1 = channels[0]; // 较大特征值lambda_1，float类型
    cv::Mat lambda2 = channels[1]; // 较小特征值lambda_2

    // 归一化到 0~255
    cv::normalize(lambda1, lambda1, 0, 255, cv::NORM_MINMAX);
    cv::normalize(lambda2, lambda2, 0, 255, cv::NORM_MINMAX);
    lambda1.convertTo(lambda1, CV_8U);
    lambda2.convertTo(lambda2, CV_8U);

    // 显示：使用 fitToScreen 限制显示尺寸
    cv::imshow("Lambda1", fitToScreen(lambda1, 800));
    cv::imshow("Lambda2", fitToScreen(lambda2, 800));

    // Shi-Tomasi 角点二值图
    // 直接使用两个特征值中较小的那个作为角点强度。一个像素只有在两个方向上都存在显著变化（两个特征值都足够大）时才被认为是角点，而最小特征值恰好能衡量"较弱方向的变化量"。
    // 前面 已将lambda2线性映射到 [0, 255]，最强的角点响应变为 255，最弱的变为 0。故下面取选择最强角点响应的 1% （0.01 * 255）作为阈值。
    cv::Mat corners;
    cv::threshold(lambda2, corners, 0.01 * 255, 255, cv::THRESH_BINARY);
    // 其实等价于cv::goodFeaturesToTrack(gray, corners, maxCorners, qualityLevel, minDistance); // qualityLevel 即我们上面设置的0.01，
    // 当然goodFeaturesToTrack()额外做了非极大值抑制，而我们没有，故因此可能在角点附近产生多个相邻响应。
    cv::imshow("Shi-Tomasi corners", fitToScreen(corners, 800));

    // 特征向量方向图
    cv::Mat rgb;
    cv::cvtColor(gray, rgb, cv::COLOR_GRAY2BGR);
    for (int y = 0; y < eigen.rows; y += 10) {
        for (int x = 0; x < eigen.cols; x += 10) {
            const float* p = eigen.ptr<float>(y, x);
            float v2x = p[4], v2y = p[5];
            cv::Point pt(x, y);
            cv::line(rgb, pt, pt + cv::Point(v2x * 5, v2y * 5),
                cv::Scalar(0, 255, 0), 1);
        }
    }
    cv::imshow("Eigenvectors", fitToScreen(rgb, 800));

    cv::waitKey(0);
}

#endif // INC_IMAGEREG_HARRISDETECT_H_
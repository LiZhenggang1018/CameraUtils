/*
 * @Description: Reg CoverFrame or TransitionFrame to LastVidFrame in LivePhoto
 */

#ifndef LPREG_H_
#define LPREG_H_

#include <filesystem>

#include "FFmpegDefine.h"
#include "YUVImageReader.h"
#include "ImageRegistrator.h"

namespace fs = std::filesystem;

// 全局且 inline 的 H 矩阵（video -> cover）
inline cv::Mat g_hMatrixVidToCover;

inline std::pair<double, double> DumpXYScaleFroMatrixVidToCover() // 写入txt
{
    if (g_hMatrixVidToCover.empty() || (g_hMatrixVidToCover.rows * g_hMatrixVidToCover.cols) < 5)
    {
        return {};
    }

    // 确保以 double 读取矩阵元素（避免类型不匹配导致 at<double> 抛异常）
    cv::Mat mat64;
    if (g_hMatrixVidToCover.type() != CV_64F)
    {
        g_hMatrixVidToCover.convertTo(mat64, CV_64F);
    }
    else
    {
        mat64 = g_hMatrixVidToCover;
    }

    // 下标 0 -> (0,0)，下标 4 -> (1,1)
    double x = 0.0;
    double y = 0.0;
    try
    {
        x = mat64.at<double>(0, 0);
        y = mat64.at<double>(1, 1);
    }
    catch (...)
    {
        // 读取失败返回默认值
        return {};
    }

    // 将 x,y 写入当前代码路径下的 vidToVoverScaleRatio.txt
    {
        std::filesystem::path path = std::filesystem::current_path() / vidToCoverScaleRatioFile;
        std::ofstream ofs(path, std::ofstream::trunc);
        if (ofs)
        {
            // 保存高精度以避免精度丢失
            ofs << std::setprecision(17) << x << ' ' << y << '\n';
            ofs.close();
        }
        else
        {
            std::cerr << "Warning: cannot open file to write: " << path.string() << std::endl;
        }
    }

    // 从文件中读回赋值给 x1,y1
    double x1{};
    double y1{};
    {
        std::filesystem::path path = std::filesystem::current_path() / vidToCoverScaleRatioFile;
        std::ifstream ifs(path);
        if (ifs)
        {
            ifs >> x1 >> y1;
            ifs.close();
        }
        else
        {
            std::cerr << "Warning: cannot open file to read: " << path.string() << std::endl;
        }
    }

    PV("DumpXYScaleFroMatrixVidToCover:", x, y);
    PV("DumpXYScaleFroMatrixVidToCover readback:", x1, y1);

    return {x, y};
}

inline std::pair<double, double> GetXYScaleFroMatrixVidToCover()
{
    // 从文件中读回赋值给 x1,y1
    double x = 0.0;
    double y = 0.0;
    {
        std::filesystem::path path = std::filesystem::current_path() / vidToCoverScaleRatioFile;
        std::ifstream ifs(path);
        if (ifs)
        {
            ifs >> x >> y;
            ifs.close();
        }
        else
        {
            std::cerr << "Warning: cannot open file to read: " << path.string() << std::endl;
        }
    }
    PV("GetXYScaleFromatrixVidToCover:", x, y);
    return {x, y};
}

inline int RegCoverFrameToLastVidFrame(int width, int height, const std::string &refYuv, const std::string &targetYuv, const std::string &outDir)
{
    try
    {
        const FeatureDetectorType DETECTOR_TYPE = FeatureDetectorType::ORB; // 特征检测器类型
        const TransformType TRANSFORM_TYPE = TransformType::HOMOGRAPHY;     // 变换类型

        // ============================================
        // 程序主体（参数由函数形参提供）
        // ============================================

        std::cout << "=================================" << std::endl;
        std::cout << "YUV420P图像配准程序开始" << std::endl;
        std::cout << "=================================" << std::endl;

        // 打印配置信息
        std::cout << "\n配置参数:" << std::endl;
        std::cout << "---------------------------------" << std::endl;
        std::cout << "图像尺寸: " << width << "x" << height << std::endl;
        std::cout << "参考图像: " << refYuv << std::endl;
        std::cout << "目标图像: " << targetYuv << std::endl;
        std::cout << "输出目录: " << outDir << std::endl;

        switch (DETECTOR_TYPE)
        {
        case FeatureDetectorType::ORB:
            std::cout << "特征检测器: ORB" << std::endl;
            break;
        case FeatureDetectorType::BRISK:
            std::cout << "特征检测器: BRISK" << std::endl;
            break;
        case FeatureDetectorType::KAZE:
            std::cout << "特征检测器: KAZE" << std::endl;
            break;
        case FeatureDetectorType::AKAZE:
            std::cout << "特征检测器: AKAZE" << std::endl;
            break;
        }


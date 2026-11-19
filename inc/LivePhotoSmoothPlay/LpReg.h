/*
 * @Description: Reg CoverFrame or TransitionFrame to LastVidFrame in LivePhoto
 */

#ifndef INC_LIVEPHOTOSMOOTHPLAY_LPREG_H
#define INC_LIVEPHOTOSMOOTHPLAY_LPREG_H

#include <filesystem>

#include "DecAndEnc.h"
#include "FFmpegDefine.h"
#include "ImageRegistrator.h"
#include "YUVImageReader.h"

namespace fs = std::filesystem;

// 全局且 inline 的 H 矩阵（video -> cover）
inline cv::Mat g_hMatrixVidToCover;

inline std::pair<double, double> DumpXYScaleFroMatrixVidToCover() // 写入txt
{
    if (g_hMatrixVidToCover.empty() || (g_hMatrixVidToCover.rows * g_hMatrixVidToCover.cols) < 5) {
        return {};
    }

    // 确保以 double 读取矩阵元素（避免类型不匹配导致 at<double> 抛异常）
    cv::Mat mat64;
    if (g_hMatrixVidToCover.type() != CV_64F) {
        g_hMatrixVidToCover.convertTo(mat64, CV_64F);
    } else {
        mat64 = g_hMatrixVidToCover;
    }

    // 下标 0 -> (0,0)，下标 4 -> (1,1)
    double x = 0.0;
    double y = 0.0;
    try {
        x = mat64.at<double>(0, 0);
        y = mat64.at<double>(1, 1);
    } catch (...) {
        // 读取失败返回默认值
        return {};
    }

    // 将 x,y 写入当前代码路径下的 vidToVoverScaleRatio.txt
    {
        std::filesystem::path path = std::filesystem::current_path() / VID_TO_COVER_SCALE_RATIO_FILE;
        std::ofstream ofs(path, std::ofstream::trunc);
        if (ofs) {
            // 保存高精度以避免精度丢失
            ofs << std::setprecision(17) << x << ' ' << y << '\n';
            ofs.close();
        } else {
            std::cerr << "Warning: cannot open file to write: " << path.string() << std::endl;
        }
    }

    // 从文件中读回赋值给 x1,y1
    double x1{};
    double y1{};
    {
        std::filesystem::path path = std::filesystem::current_path() / VID_TO_COVER_SCALE_RATIO_FILE;
        std::ifstream ifs(path);
        if (ifs) {
            ifs >> x1 >> y1;
            ifs.close();
        } else {
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
        std::filesystem::path path = std::filesystem::current_path() / VID_TO_COVER_SCALE_RATIO_FILE;
        std::ifstream ifs(path);
        if (ifs) {
            ifs >> x >> y;
            ifs.close();
        } else {
            std::cerr << "Warning: cannot open file to read: " << path.string() << std::endl;
        }
    }
    PV("GetXYScaleFromatrixVidToCover:", x, y);
    return {x, y};
}

inline int RegCoverFrameToLastVidFrame(int width, int height, const std::string& refYuv, const std::string& targetYuv,
                                       const std::string& outDir)
{
    try {
        const FeatureDetectorType detectorType = FeatureDetectorType::SIFT; // 特征检测器类型
        const TransformType transformType = TransformType::HOMOGRAPHY;      // 变换类型

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

        switch (detectorType) {
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

        switch (transformType) {
        case TransformType::HOMOGRAPHY:
            std::cout << "变换类型: 单应性变换" << std::endl;
            break;
        case TransformType::AFFINE:
            std::cout << "变换类型: 仿射变换" << std::endl;
            break;
        case TransformType::EUCLIDEAN:
            std::cout << "变换类型: 欧几里得变换" << std::endl;
            break;
        case TransformType::TRANSLATION:
            std::cout << "变换类型: 平移变换" << std::endl;
            break;
        }
        std::cout << "=================================" << std::endl;

        // 创建输出目录
        std::filesystem::create_directories(outDir);

        // 1. 创建YUV读取器
        std::cout << "\n步骤1: 初始化YUV读取器..." << std::endl;
        YUVImageReader yuvReader(width, height);

        // 2. 读取YUV图像
        std::cout << "\n步骤2: 读取YUV图像文件..." << std::endl;
        std::cout << "读取参考图像: " << refYuv << std::endl;
        cv::Mat referenceImg = yuvReader.ReadYuv420P(refYuv);

        std::cout << "读取目标图像: " << targetYuv << std::endl;
        cv::Mat targetImg = yuvReader.ReadYuv420P(targetYuv);

        if (referenceImg.empty()) {
            throw std::runtime_error("参考图像读取失败或为空: " + refYuv);
        }
        if (targetImg.empty()) {
            throw std::runtime_error("目标图像读取失败或为空: " + targetYuv);
        }

        std::cout << "图像读取成功!" << std::endl;
        std::cout << "参考图像尺寸: " << referenceImg.cols << "x" << referenceImg.rows
                  << ", 通道数: " << referenceImg.channels() << std::endl;
        std::cout << "目标图像尺寸: " << targetImg.cols << "x" << targetImg.rows << ", 通道数: " << targetImg.channels()
                  << std::endl;

        // 保存原始图像（用于对比）
        std::cout << "\n步骤3: 保存原始图像用于对比..." << std::endl;
        cv::imwrite(outDir + "/01_reference_original.jpg", referenceImg);
        cv::imwrite(outDir + "/02_target_original.jpg", targetImg);
        std::cout << "原始图像已保存到输出目录" << std::endl;

        // 3. 创建配准器
        ImageRegistrator registrator;

        // 4. 执行配准
        std::cout << "\n步骤4: 开始图像配准..." << std::endl;
        auto result = registrator.RegisterImages(referenceImg, targetImg, detectorType, transformType);

        // 检查配准是否成功
        if (result.registeredImage.empty()) {
            throw std::runtime_error("图像配准失败，配准结果为空");
        }

        if (result.transformMatrix.empty()) {
            throw std::runtime_error("图像配准失败，变换矩阵为空");
        }

        // 在将 H 矩阵写入 transform_matrix.txt 之前，赋值给全局变量 g_hMatrixVidToCover
        if (!result.transformMatrix.empty()) {
            // 使用 clone() 确保独立拷贝
            g_hMatrixVidToCover = result.transformMatrix.clone();
            DumpXYScaleFroMatrixVidToCover();
        } else {
            PV("clone transform_matrix fail");
            g_hMatrixVidToCover.release();
        }

        // 5. 保存配准结果
        std::cout << "\n步骤5: 保存配准结果..." << std::endl;
        registrator.SaveRegistrationResults(result, outDir);

        // 6. 绘制匹配结果
        std::cout << "\n步骤6: 绘制特征匹配结果..." << std::endl;
        cv::Mat matchesImg =
            registrator.DrawMatches(referenceImg, targetImg, result.keypoints1, result.keypoints2, result.goodMatches);

        cv::imwrite(outDir + "/03_feature_matches.jpg", matchesImg);

        // 7. 创建融合图像以展示配准效果
        std::cout << "\n步骤7: 创建配准效果对比图..." << std::endl;

        // 7.1 创建叠加图（半透明叠加）
        cv::Mat overlayImg;
        cv::addWeighted(referenceImg, 0.5, result.registeredImage, 0.5, 0, overlayImg);
        cv::imwrite(outDir + "/04_overlay_result.jpg", overlayImg);

        // 7.2 创建并排对比图
        cv::Mat sideBySide;
        cv::hconcat(referenceImg, result.registeredImage, sideBySide);
        cv::imwrite(outDir + "/05_side_by_side.jpg", sideBySide);

        // 7.3 创建差异图
        cv::Mat diffImg;
        cv::absdiff(referenceImg, result.registeredImage, diffImg);
        cv::imwrite(outDir + "/06_difference.jpg", diffImg);

        // 8. 保存配准后的YUV格式
        std::cout << "\n步骤8: 保存配准后的YUV格式..." << std::endl;
        yuvReader.SaveAsYuv420P(result.registeredImage, outDir + "/07_registered_image.yuv");

        // 9. 保存变换矩阵图像（可视化）
        std::cout << "\n步骤9: 生成配准报告..." << std::endl;

        // 创建配准报告图像
        cv::Mat reportImg(800, 1200, CV_8UC3, cv::Scalar(240, 240, 240));

        // 添加标题
        cv::putText(reportImg, "YUV420P图像配准报告", cv::Point(50, 50), cv::FONT_HERSHEY_DUPLEX, 1.5,
                    cv::Scalar(0, 0, 0), 2);

        // 添加配准统计信息
        std::string statsText = "配准统计信息:";
        cv::putText(reportImg, statsText, cv::Point(50, 100), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 0), 1);

        std::vector<std::string> statsLines = {"图像尺寸: " + std::to_string(width) + "x" + std::to_string(height),
                                               "匹配点数量: " + std::to_string(result.goodMatches.size()),
                                               "内点数量: " + std::to_string(result.numInliers),
                                               "RMS误差: " + std::to_string(result.rmsError) + " 像素"};

        int yOffset = 130;
        for (const auto& line : statsLines) {
            cv::putText(reportImg, line, cv::Point(70, yOffset), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(50, 50, 50),
                        1);
            yOffset += 25;
        }

        // 添加变换矩阵
        cv::putText(reportImg, "变换矩阵:", cv::Point(50, yOffset + 20), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                    cv::Scalar(0, 0, 0), 1);

        if (!result.transformMatrix.empty()) {
            yOffset += 50;
            for (int i = 0; i < result.transformMatrix.rows; i++) {
                std::string rowStr;
                for (int j = 0; j < result.transformMatrix.cols; j++) {
                    rowStr += std::to_string(result.transformMatrix.at<double>(i, j));
                    if (j < result.transformMatrix.cols - 1) {
                        rowStr += ", ";
                    }
                }
                cv::putText(reportImg, rowStr, cv::Point(70, yOffset), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                            cv::Scalar(50, 50, 50), 1);
                yOffset += 20;
            }
        }

        // 保存报告图像
        cv::imwrite(outDir + "/08_registration_report.jpg", reportImg);

        // 保存配准参数配置文件
        std::ofstream configFile(outDir + "/09_configuration.txt");
        if (configFile.is_open()) {
            configFile << "YUV420P图像配准配置参数" << std::endl;
            configFile << "=================================" << std::endl;
            configFile << "生成时间: " << __DATE__ << " " << __TIME__ << std::endl;
            configFile << std::endl;
            configFile << "1. 图像参数" << std::endl;
            configFile << "   宽度: " << width << " 像素" << std::endl;
            configFile << "   高度: " << height << " 像素" << std::endl;
            configFile << "   参考图像: " << refYuv << std::endl;
            configFile << "   目标图像: " << targetYuv << std::endl;
            configFile << std::endl;
            configFile << "2. 算法参数" << std::endl;
            configFile << "   特征检测器: ";
            switch (detectorType) {
            case FeatureDetectorType::ORB:
                configFile << "ORB";
                break;
            case FeatureDetectorType::BRISK:
                configFile << "BRISK";
                break;
            case FeatureDetectorType::KAZE:
                configFile << "KAZE";
                break;
            case FeatureDetectorType::AKAZE:
                configFile << "AKAZE";
                break;
            }
            configFile << std::endl;
            configFile << "   变换类型: ";
            switch (transformType) {
            case TransformType::HOMOGRAPHY:
                configFile << "单应性变换";
                break;
            case TransformType::AFFINE:
                configFile << "仿射变换";
                break;
            case TransformType::EUCLIDEAN:
                configFile << "欧几里得变换";
                break;
            case TransformType::TRANSLATION:
                configFile << "平移变换";
                break;
            }
            configFile << std::endl;
            configFile << std::endl;
            configFile << "3. 配准结果" << std::endl;
            configFile << "   匹配点数量: " << result.goodMatches.size() << std::endl;
            configFile << "   内点数量: " << result.numInliers << std::endl;
            configFile << "   RMS误差: " << result.rmsError << " 像素" << std::endl;
            configFile << std::endl;
            configFile << "4. 输出文件说明" << std::endl;
            configFile << "   01_reference_original.jpg - 参考图像" << std::endl;
            configFile << "   02_target_original.jpg - 目标图像" << std::endl;
            configFile << "   03_feature_matches.jpg - 特征匹配可视化" << std::endl;
            configFile << "   04_overlay_result.jpg - 配准叠加效果" << std::endl;
            configFile << "   05_side_by_side.jpg - 并排对比" << std::endl;
            configFile << "   06_difference.jpg - 差异图" << std::endl;
            configFile << "   07_registered_image.yuv - 配准后的YUV文件" << std::endl;
            configFile << "   08_registration_report.jpg - 配准报告" << std::endl;
            configFile << "   09_configuration.txt - 本配置文件" << std::endl;
            configFile << "   transform_matrix.txt - 变换矩阵数据" << std::endl;
            configFile << "   matches.txt - 匹配点详细数据" << std::endl;
            configFile.close();
        }

        // ============================================
        // 程序完成
        // ============================================

        std::cout << "\n=================================" << std::endl;
        std::cout << "图像配准完成！" << std::endl;
        std::cout << "=================================" << std::endl;

        std::cout << "\n配准统计信息:" << std::endl;
        std::cout << "---------------------------------" << std::endl;
        std::cout << "特征点匹配数量: " << result.goodMatches.size() << std::endl;
        std::cout << "内点数量: " << result.numInliers << std::endl;
        std::cout << "RMS误差: " << result.rmsError << " 像素" << std::endl;
        std::cout << "=================================" << std::endl;

        std::cout << "\n输出文件已保存到目录: " << outDir << std::endl;
        std::cout << "1. 01_reference_original.jpg - 参考图像" << std::endl;
        std::cout << "2. 02_target_original.jpg - 目标图像" << std::endl;
        std::cout << "3. 03_feature_matches.jpg - 特征匹配可视化" << std::endl;
        std::cout << "4. 04_overlay_result.jpg - 配准叠加效果" << std::endl;
        std::cout << "5. 05_side_by_side.jpg - 并排对比" << std::endl;
        std::cout << "6. 06_difference.jpg - 差异图" << std::endl;
        std::cout << "7. 07_registered_image.yuv - 配准后的YUV文件" << std::endl;
        std::cout << "8. 08_registration_report.jpg - 配准报告" << std::endl;
        std::cout << "9. 09_configuration.txt - 配置文件" << std::endl;
        std::cout << "10. transform_matrix.txt - 变换矩阵数据" << std::endl;
        std::cout << "11. matches.txt - 匹配点详细数据" << std::endl;
        std::cout << "=================================" << std::endl;

        std::cout << "\n注意：" << std::endl;
        std::cout << "1. 确保输入YUV文件路径正确且格式为YUV420P" << std::endl;
        std::cout << "2. 图像尺寸必须与代码中指定的" << width << "x" << height << "一致" << std::endl;
        std::cout << "3. 如需修改参数，请直接编辑main.cpp中的硬编码值" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n=================================" << std::endl;
        std::cerr << "程序执行出错!" << std::endl;
        std::cerr << "错误信息: " << e.what() << std::endl;
        std::cerr << "=================================" << std::endl;

        std::cerr << "\n可能的原因:" << std::endl;
        std::cerr << "1. YUV文件路径不正确或文件不存在" << std::endl;
        std::cerr << "2. 图像尺寸与YUV文件实际尺寸不匹配" << std::endl;
        std::cerr << "3. YUV文件格式不是YUV420P" << std::endl;
        std::cerr << "4. 磁盘空间不足或没有写入权限" << std::endl;
        std::cerr << "=================================" << std::endl;
        return 1;
    }

    return 0;
}

inline void RegCoverToVid()
{
    RegCoverFrameToLastVidFrame(YUV_W, YUV_H, LAST_VID_FRAME_YUV, COVER_FRAME_YUV, REG_RESULT_DIR);
}

// 由于录屏文件中，LP原始图像只占据中部分区域，通过手动从图像中crop出该区域，
inline void DecodeCropEncodeInputVid()
{
    constexpr double xCropRatio{};
    constexpr double ratio = static_cast<double>(1260 - 300) / 1562;
    constexpr double yCropRatio = 1 - ratio;
    DecodeCropEncode(LP_INIT_FILE.c_str(), LP_CROP_FILE.c_str(), xCropRatio, yCropRatio);
}

#endif // INC_LIVEPHOTOSMOOTHPLAY_LPREG_H
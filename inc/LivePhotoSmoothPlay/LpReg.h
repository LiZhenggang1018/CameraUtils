/*
 * @Description: Reg CoverFrame or TransitionFrame to LastVidFrame in LivePhoto
 */

#ifndef LPREG_H_
#define LPREG_H_

#include <filesystem>

#include "FFmpegDefine.h"
#include "YUVImageReader.h"
#include "ImageRegistrator.h"
#include "DecAndEnc.h"

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
        const FeatureDetectorType DETECTOR_TYPE = FeatureDetectorType::SIFT; // 特征检测器类型
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

        switch (TRANSFORM_TYPE)
        {
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
        YUVImageReader yuv_reader(width, height);

        // 2. 读取YUV图像
        std::cout << "\n步骤2: 读取YUV图像文件..." << std::endl;
        std::cout << "读取参考图像: " << refYuv << std::endl;
        cv::Mat reference_img = yuv_reader.readYUV420P(refYuv);

        std::cout << "读取目标图像: " << targetYuv << std::endl;
        cv::Mat target_img = yuv_reader.readYUV420P(targetYuv);

        if (reference_img.empty())
        {
            throw std::runtime_error("参考图像读取失败或为空: " + refYuv);
        }
        if (target_img.empty())
        {
            throw std::runtime_error("目标图像读取失败或为空: " + targetYuv);
        }

        std::cout << "图像读取成功!" << std::endl;
        std::cout << "参考图像尺寸: " << reference_img.cols << "x" << reference_img.rows
                  << ", 通道数: " << reference_img.channels() << std::endl;
        std::cout << "目标图像尺寸: " << target_img.cols << "x" << target_img.rows
                  << ", 通道数: " << target_img.channels() << std::endl;

        // 保存原始图像（用于对比）
        std::cout << "\n步骤3: 保存原始图像用于对比..." << std::endl;
        cv::imwrite(outDir + "/01_reference_original.jpg", reference_img);
        cv::imwrite(outDir + "/02_target_original.jpg", target_img);
        std::cout << "原始图像已保存到输出目录" << std::endl;

        // 3. 创建配准器
        ImageRegistrator registrator;

        // 4. 执行配准
        std::cout << "\n步骤4: 开始图像配准..." << std::endl;
        auto result = registrator.registerImages(
            reference_img, target_img, DETECTOR_TYPE, TRANSFORM_TYPE);

        // 检查配准是否成功
        if (result.registered_image.empty())
        {
            throw std::runtime_error("图像配准失败，配准结果为空");
        }

        if (result.transform_matrix.empty())
        {
            throw std::runtime_error("图像配准失败，变换矩阵为空");
        }

        // 在将 H 矩阵写入 transform_matrix.txt 之前，赋值给全局变量 g_hMatrixVidToCover
        if (!result.transform_matrix.empty())
        {
            // 使用 clone() 确保独立拷贝
            g_hMatrixVidToCover = result.transform_matrix.clone();
            DumpXYScaleFroMatrixVidToCover();
        }
        else
        {
            PV("clone transform_matrix fail");
            g_hMatrixVidToCover.release();
        }

        // 5. 保存配准结果
        std::cout << "\n步骤5: 保存配准结果..." << std::endl;
        registrator.saveRegistrationResults(result, outDir);

        // 6. 绘制匹配结果
        std::cout << "\n步骤6: 绘制特征匹配结果..." << std::endl;
        cv::Mat matches_img = registrator.drawMatches(
            reference_img, target_img,
            result.keypoints1, result.keypoints2,
            result.good_matches);

        cv::imwrite(outDir + "/03_feature_matches.jpg", matches_img);

        // 7. 创建融合图像以展示配准效果
        std::cout << "\n步骤7: 创建配准效果对比图..." << std::endl;

        // 7.1 创建叠加图（半透明叠加）
        cv::Mat overlay_img;
        cv::addWeighted(reference_img, 0.5, result.registered_image, 0.5, 0, overlay_img);
        cv::imwrite(outDir + "/04_overlay_result.jpg", overlay_img);

        // 7.2 创建并排对比图
        cv::Mat side_by_side;
        cv::hconcat(reference_img, result.registered_image, side_by_side);
        cv::imwrite(outDir + "/05_side_by_side.jpg", side_by_side);

        // 7.3 创建差异图
        cv::Mat diff_img;
        cv::absdiff(reference_img, result.registered_image, diff_img);
        cv::imwrite(outDir + "/06_difference.jpg", diff_img);

        // 8. 保存配准后的YUV格式
        std::cout << "\n步骤8: 保存配准后的YUV格式..." << std::endl;
        yuv_reader.saveAsYUV420P(result.registered_image, outDir + "/07_registered_image.yuv");

        // 9. 保存变换矩阵图像（可视化）
        std::cout << "\n步骤9: 生成配准报告..." << std::endl;

        // 创建配准报告图像
        cv::Mat report_img(800, 1200, CV_8UC3, cv::Scalar(240, 240, 240));

        // 添加标题
        cv::putText(report_img, "YUV420P图像配准报告", cv::Point(50, 50),
                    cv::FONT_HERSHEY_DUPLEX, 1.5, cv::Scalar(0, 0, 0), 2);

        // 添加配准统计信息
        std::string stats_text = "配准统计信息:";
        cv::putText(report_img, stats_text, cv::Point(50, 100),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 0), 1);

        std::vector<std::string> stats_lines = {
            "图像尺寸: " + std::to_string(width) + "x" + std::to_string(height),
            "匹配点数量: " + std::to_string(result.good_matches.size()),
            "内点数量: " + std::to_string(result.num_inliers),
            "RMS误差: " + std::to_string(result.rms_error) + " 像素"};

        int y_offset = 130;
        for (const auto &line : stats_lines)
        {
            cv::putText(report_img, line, cv::Point(70, y_offset),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(50, 50, 50), 1);
            y_offset += 25;
        }

        // 添加变换矩阵
        cv::putText(report_img, "变换矩阵:", cv::Point(50, y_offset + 20),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 0), 1);

        if (!result.transform_matrix.empty())
        {
            y_offset += 50;
            for (int i = 0; i < result.transform_matrix.rows; i++)
            {
                std::string row_str;
                for (int j = 0; j < result.transform_matrix.cols; j++)
                {
                    row_str += std::to_string(result.transform_matrix.at<double>(i, j));
                    if (j < result.transform_matrix.cols - 1)
                        row_str += ", ";
                }
                cv::putText(report_img, row_str, cv::Point(70, y_offset),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(50, 50, 50), 1);
                y_offset += 20;
            }
        }

        // 保存报告图像
        cv::imwrite(outDir + "/08_registration_report.jpg", report_img);

        // 保存配准参数配置文件
        std::ofstream config_file(outDir + "/09_configuration.txt");
        if (config_file.is_open())
        {
            config_file << "YUV420P图像配准配置参数" << std::endl;
            config_file << "=================================" << std::endl;
            config_file << "生成时间: " << __DATE__ << " " << __TIME__ << std::endl;
            config_file << std::endl;
            config_file << "1. 图像参数" << std::endl;
            config_file << "   宽度: " << width << " 像素" << std::endl;
            config_file << "   高度: " << height << " 像素" << std::endl;
            config_file << "   参考图像: " << refYuv << std::endl;
            config_file << "   目标图像: " << targetYuv << std::endl;
            config_file << std::endl;
            config_file << "2. 算法参数" << std::endl;
            config_file << "   特征检测器: ";
            switch (DETECTOR_TYPE)
            {
            case FeatureDetectorType::ORB:
                config_file << "ORB";
                break;
            case FeatureDetectorType::BRISK:
                config_file << "BRISK";
                break;
            case FeatureDetectorType::KAZE:
                config_file << "KAZE";
                break;
            case FeatureDetectorType::AKAZE:
                config_file << "AKAZE";
                break;
            }
            config_file << std::endl;
            config_file << "   变换类型: ";
            switch (TRANSFORM_TYPE)
            {
            case TransformType::HOMOGRAPHY:
                config_file << "单应性变换";
                break;
            case TransformType::AFFINE:
                config_file << "仿射变换";
                break;
            case TransformType::EUCLIDEAN:
                config_file << "欧几里得变换";
                break;
            case TransformType::TRANSLATION:
                config_file << "平移变换";
                break;
            }
            config_file << std::endl;
            config_file << std::endl;
            config_file << "3. 配准结果" << std::endl;
            config_file << "   匹配点数量: " << result.good_matches.size() << std::endl;
            config_file << "   内点数量: " << result.num_inliers << std::endl;
            config_file << "   RMS误差: " << result.rms_error << " 像素" << std::endl;
            config_file << std::endl;
            config_file << "4. 输出文件说明" << std::endl;
            config_file << "   01_reference_original.jpg - 参考图像" << std::endl;
            config_file << "   02_target_original.jpg - 目标图像" << std::endl;
            config_file << "   03_feature_matches.jpg - 特征匹配可视化" << std::endl;
            config_file << "   04_overlay_result.jpg - 配准叠加效果" << std::endl;
            config_file << "   05_side_by_side.jpg - 并排对比" << std::endl;
            config_file << "   06_difference.jpg - 差异图" << std::endl;
            config_file << "   07_registered_image.yuv - 配准后的YUV文件" << std::endl;
            config_file << "   08_registration_report.jpg - 配准报告" << std::endl;
            config_file << "   09_configuration.txt - 本配置文件" << std::endl;
            config_file << "   transform_matrix.txt - 变换矩阵数据" << std::endl;
            config_file << "   matches.txt - 匹配点详细数据" << std::endl;
            config_file.close();
        }

        // ============================================
        // 程序完成
        // ============================================

        std::cout << "\n=================================" << std::endl;
        std::cout << "图像配准完成！" << std::endl;
        std::cout << "=================================" << std::endl;

        std::cout << "\n配准统计信息:" << std::endl;
        std::cout << "---------------------------------" << std::endl;
        std::cout << "特征点匹配数量: " << result.good_matches.size() << std::endl;
        std::cout << "内点数量: " << result.num_inliers << std::endl;
        std::cout << "RMS误差: " << result.rms_error << " 像素" << std::endl;
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
    }
    catch (const std::exception &e)
    {
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
    constexpr double xCropRatio{ };
    constexpr double ratio = static_cast<double>(1260 - 300) / 1562;
    constexpr double yCropRatio = 1- ratio;
    DecodeCropEncode(LP_INIT_FILE.c_str(), LP_CROP_FILE.c_str(), xCropRatio, yCropRatio);
}

#endif // LPREG_H_
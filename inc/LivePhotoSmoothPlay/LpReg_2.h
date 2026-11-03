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

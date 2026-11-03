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

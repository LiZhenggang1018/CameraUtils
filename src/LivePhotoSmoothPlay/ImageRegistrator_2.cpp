            warp_to_ref = inv;
        }
        else
        {
            throw std::runtime_error("不支持的变换矩阵维度");
        }
    }
    else
    {
        throw std::runtime_error("估计出的变换矩阵为空");
    }

    // 4. 应用（对目标图像应用 target->reference 的变换）
    std::cout << "应用变换..." << std::endl;
    result.registered_image = applyTransform(target_img, warp_to_ref, reference_img.size());

    // 计算RMS误差（使用相同方向：把目标点通过 warp_to_ref 映射到参考图像坐标，与参考点比较）
    result.rms_error = 0.0;
    if (!inlier_matches.empty() && !warp_to_ref.empty())
    {
        for (const auto &match : inlier_matches)
        {
            cv::Point2f pt_ref = keypoints1[match.queryIdx].pt; // reference
            cv::Point2f pt_tar = keypoints2[match.trainIdx].pt; // target

            cv::Mat pt_tar_homog = (cv::Mat_<double>(3, 1) << pt_tar.x, pt_tar.y, 1);
            cv::Mat pt_tar_transformed = warp_to_ref * pt_tar_homog;
            pt_tar_transformed /= pt_tar_transformed.at<double>(2, 0);

            double dx = pt_ref.x - pt_tar_transformed.at<double>(0, 0);
            double dy = pt_ref.y - pt_tar_transformed.at<double>(1, 0);
            result.rms_error += dx * dx + dy * dy;
        }
        result.rms_error = std::sqrt(result.rms_error / inlier_matches.size());
    }

    result.keypoints1 = std::move(keypoints1);
    result.keypoints2 = std::move(keypoints2);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "配准完成，耗时: " << duration.count() << "ms" << std::endl;
    std::cout << "RMS误差: " << result.rms_error << " 像素" << std::endl;

    return result;
}

cv::Mat ImageRegistrator::drawMatches(
    const cv::Mat &img1,
    const cv::Mat &img2,
    const std::vector<cv::KeyPoint> &keypoints1,
    const std::vector<cv::KeyPoint> &keypoints2,
    const std::vector<cv::DMatch> &matches)
{

    cv::Mat matches_img;
    cv::drawMatches(img1, keypoints1, img2, keypoints2, matches,
                    matches_img, cv::Scalar::all(-1), cv::Scalar::all(-1),
                    std::vector<char>(), cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);

    return matches_img;
}

void ImageRegistrator::saveRegistrationResults(const RegistrationResult &result, const std::string &output_dir)
{

    // 创建输出目录
    std::filesystem::create_directories(output_dir);

    // 保存配准后的图像
    std::string output_path = output_dir + "/registered_image.jpg";
    cv::imwrite(output_path, result.registered_image);

    // 保存变换矩阵
    std::string transform_path = output_dir + "/transform_matrix.txt";
    std::ofstream transform_file(transform_path);
    if (transform_file.is_open())
    {
        transform_file << "变换矩阵:\n";
        transform_file << result.transform_matrix << std::endl;
        transform_file << "\n配准参数:\n";
        transform_file << "内点数: " << result.num_inliers << std::endl;
        transform_file << "RMS误差: " << result.rms_error << " 像素" << std::endl;
        transform_file.close();
    }

    // 保存匹配点信息
    std::string matches_path = output_dir + "/matches.txt";
    std::ofstream matches_file(matches_path);
    if (matches_file.is_open())
    {
        matches_file << "匹配点数量: " << result.good_matches.size() << std::endl;
        matches_file << "\n匹配点详细信息:\n";
        matches_file << "索引, 参考图像X, 参考图像Y, 目标图像X, 目标图像Y, 距离\n";

        for (size_t i = 0; i < result.good_matches.size(); ++i)
        {
            const auto &match = result.good_matches[i];
            cv::Point2f pt1 = result.keypoints1[match.queryIdx].pt;
            cv::Point2f pt2 = result.keypoints2[match.trainIdx].pt;

            matches_file << i << ", " << pt1.x << ", " << pt1.y << ", "
                         << pt2.x << ", " << pt2.y << ", "
                         << match.distance << std::endl;
        }
        matches_file.close();
    }
}
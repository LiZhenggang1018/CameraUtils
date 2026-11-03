}

// 由于录屏文件中，LP原始图像只占据中部分区域，通过手动从图像中crop出该区域，
inline void DecodeCropEncodeInputVid()
{
    constexpr double xCropRatio{ };
    constexpr double ratio = static_cast<double>(1260 - 300) / 1562;
    constexpr double yCropRatio = 1- ratio;
    DecodeCropEncode(LP_INIT_FILE.c_str(), LP_CROP_FILE.c_str(), xCropRatio, yCropRatio);
}

// 目标：对 crop1.yuv .. crop20.yuv 每个与 LAST_VID_FRAME_YUV（参考）做配准，
// 每次配准结果保存到 registration_results_N 文件夹，然后从每个文件夹拷贝 registered_image.yuv 和 registered_image.jpg
// 到当前路径并重命名为 cropReg_N.yuv / cropReg_N.jpg
inline void RegCroppedConverYuvsToVidFrame()
{
    for (int n = 1; n <= TransFrameNum; ++n)
    {
        // 构造 crop 文件名（假定在当前工作目录）
        std::string cropFilename = CROP_COVER_DIR + "/crop" + std::to_string(n) + ".yuv";

        // 输出目录：registration_results_1 ... registration_results_N
        std::string outDir = std::string(CROP_REG_RESULT_DIR) + "_" + std::to_string(n);

        // 确保输出目录存在
        std::error_code ec;
        if (!fs::exists(outDir))
        {
            if (!fs::create_directories(outDir, ec))
            {
                std::cerr << "Warning: 无法创建目录 " << outDir << ", 错误: " << ec.message() << std::endl;
            }
        }

        std::cout << "=== 配准: " << cropFilename << " -> " << LAST_VID_FRAME_YUV << " 输出目录: " << outDir << " ===" << std::endl;

        // 调用现有配准函数（参考帧：LAST_VID_FRAME_YUV，目标帧：cropFilename）
        int rc = RegCoverFrameToLastVidFrame(YUV_W, YUV_H, LAST_VID_FRAME_YUV, cropFilename, outDir);

        if (rc != 0)
        {
            std::cerr << "Warning: RegCoverFrameToLastVidFrame 返回非0 (" << rc << ")，仍尝试拷贝可能的输出文件。" << std::endl;
        }

        // 确保 AllCopyCropRegged 目录存在（放置所有拷贝的配准结果）
        std::filesystem::path allCopyDir = std::filesystem::current_path() / "AllCopyCropRegged";
        std::error_code mkdir_ec;
        if (!std::filesystem::exists(allCopyDir))
        {
            if (!std::filesystem::create_directories(allCopyDir, mkdir_ec))
            {
                std::cerr << "Warning: 无法创建目录 " << allCopyDir.string() << ", 错误: " << mkdir_ec.message() << std::endl;
            }
        }

        // 拷贝 registered_image.jpg（ImageRegistrator::saveRegistrationResults 保存为 registered_image.jpg）
        fs::path src_jpg = fs::path(outDir) / "registered_image.jpg";
        std::filesystem::path dest_jpg = allCopyDir / ("cropReg_" + std::to_string(n) + ".jpg");
        if (fs::exists(src_jpg))
        {
            std::error_code copy_ec;
            fs::copy_file(src_jpg, dest_jpg, fs::copy_options::overwrite_existing, copy_ec);
            if (copy_ec)
            {
                std::cerr << "Warning: 复制 " << src_jpg.string() << " 到 " << dest_jpg.string() << " 失败: " << copy_ec.message() << std::endl;
            }
        }
        else
        {
            // 有些流程可能把 JPG 名称放在不同文件名，尝试一些常见备选名（保持谨慎）
            fs::path alt_jpg = fs::path(outDir) / ("crop" + std::to_string(n) + ".jpg");
            if (fs::exists(alt_jpg))
            {
                std::error_code copy_ec;
                fs::copy_file(alt_jpg, dest_jpg, fs::copy_options::overwrite_existing, copy_ec);
                if (copy_ec)
                {
                    std::cerr << "Warning: 复制 " << alt_jpg.string() << " 到 " << dest_jpg.string() << " 失败: " << copy_ec.message() << std::endl;
                }
            }
            else
            {
                std::cerr << "Warning: 未找到 JPG 文件于 " << outDir << std::endl;
            }
        }

        // 拷贝 registered_image.yuv：LpReg 中保存为 "07_registered_image.yuv"（优先）或可能为 "registered_image.yuv"
        fs::path src_yuv_primary = fs::path(outDir) / "07_registered_image.yuv";
        fs::path src_yuv_secondary = fs::path(outDir) / "registered_image.yuv";
        std::filesystem::path dest_yuv = allCopyDir / ("cropReg_" + std::to_string(n) + ".yuv");

        if (fs::exists(src_yuv_primary))
        {
            std::error_code copy_ec;
            fs::copy_file(src_yuv_primary, dest_yuv, fs::copy_options::overwrite_existing, copy_ec);
            if (copy_ec)
            {
                std::cerr << "Warning: 复制 " << src_yuv_primary.string() << " 到 " << dest_yuv.string() << " 失败: " << copy_ec.message() << std::endl;
            }
        }
        else if (fs::exists(src_yuv_secondary))
        {
            std::error_code copy_ec;
            fs::copy_file(src_yuv_secondary, dest_yuv, fs::copy_options::overwrite_existing, copy_ec);
            if (copy_ec)
            {
                std::cerr << "Warning: 复制 " << src_yuv_secondary.string() << " 到 " << dest_yuv.string() << " 失败: " << copy_ec.message() << std::endl;
            }
        }
        else
        {
            std::cerr << "Warning: 未找到 YUV 文件于 " << outDir << " (尝试 " << src_yuv_primary.string() << " 和 " << src_yuv_secondary.string() << ")" << std::endl;
        }

        std::cout << "Finished processing crop " << n << " (rc=" << rc << ")." << std::endl;
    }
}

#endif // LPREG_H_
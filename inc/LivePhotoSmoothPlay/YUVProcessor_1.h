        std::ofstream file(filepath, std::ios::binary);
        if (!file)
        {
            std::cerr << "无法创建输出文件: " << filepath << std::endl;
            return false;
        }

        file.write(reinterpret_cast<const char *>(data.data()), data.size());
        if (!file)
        {
            std::cerr << "写入文件失败: " << filepath << std::endl;
            return false;
        }

        return true;
    }
};

// 从封面帧生成一序列FOV渐变的过渡帧
// 当前代码路径下，读取 60.yuv，然后 生成 10 个 new1.yuv （最大FOV，跟60.yuv最接近，放在视频倒第二帧） .. new10.yuv（最小，放在视频前面）
inline int ConvertCoverFrameToMultiFovYuvs()
{
    const int width = 1920;
    const int height = 1080;
    const std::string input_file = "60.yuv"; // 需要客制化

    // 创建YUV处理器
    YUVProcessor processor(width, height);

    // 读取输入文件
    if (!processor.readYUVFile(input_file))
    {
        std::cerr << "读取输入文件失败" << std::endl;
        return 1;
    }

    // 生成10个新文件
    for (int n = 1; n <= 10; ++n)
    {
        // 计算裁剪比例（每边n%）
        float crop_percent = static_cast<float>(n);

        // 生成输出文件名
        std::string output_file = "new" + std::to_string(n) + ".yuv";

        std::cout << "处理第 " << n << " 个文件: " << output_file
                  << ", 裁剪比例: " << crop_percent << "%" << std::endl;

        // 裁剪并生成新文件
        if (!processor.cropAndResize(crop_percent, output_file))
        {
            std::cerr << "处理文件失败: " << output_file << std::endl;
            return 1;
        }

        std::cout << "文件生成成功: " << output_file << std::endl;
    }

    std::cout << "\n所有10个文件已成功生成！" << std::endl;
    return 0;
}

#endif // YUVPROCESSOR_H_
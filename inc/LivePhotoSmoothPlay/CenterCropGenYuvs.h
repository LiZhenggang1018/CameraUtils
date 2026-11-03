#ifndef CENTERCROPGENYUVS_H_
#define CENTERCROPGENYUVS_H_

#include <filesystem>

#include "OpenCVCommon.h"
#include "LpReg.h"
namespace fs = std::filesystem;

/**
 * @brief 生成中心裁剪的YUV420P图像
 *
 * @param inputYuv 输入YUV文件路径
 * @param w YUV图像宽度
 * @param h YUV图像高度
 * @param num 生成图像数量
 * @param cropRatio 单次裁剪比例
 *
 * 功能说明：
 * 1. 根据输入YUV文件生成num张相同格式相同宽高的YUV图片
 * 2. 第N张图片上下左右各裁剪N*cropRatio比例的像素
 * 3. 裁剪区域以图像中心为基准
 * 4. 输出文件命名为crop1.yuv, crop2.yuv, ..., cropN.yuv
 */
inline void CenterCropGenYuvs(const std::string &inputYuv, int w, int h, int num, double cropRatio, const std::string &outDir)
{
    // 参数校验
    if (cropRatio <= 0.0 || cropRatio > 0.5)
    {
        std::cerr << "Error: cropRatio must be between 0 and 0.5" << std::endl;
        return;
    }

    if (num <= 0)
    {
        std::cerr << "Error: num must be positive" << std::endl;
        return;
    }

    if (w <= 0 || h <= 0)
    {
        std::cerr << "Error: width and height must be positive" << std::endl;
        return;
    }

    // 确保输出目录存在（若 outDir 为空则使用当前目录）
    fs::path out_path;
    if (outDir.empty())
    {
        out_path = fs::current_path();
    }
    else
    {
        out_path = fs::path(outDir);
    }
    std::error_code ec;
    if (!fs::exists(out_path))
    {
        if (!fs::create_directories(out_path, ec))
        {
            if (ec)
            {
                std::cerr << "Error: failed to create output directory: " << out_path.string() << " (" << ec.message() << ")" << std::endl;
                return;
            }
        }
    }

    // 打开输入YUV文件
    std::ifstream inputFile(inputYuv, std::ios::binary);
    if (!inputFile.is_open())
    {
        std::cerr << "Error: Cannot open input YUV file: " << inputYuv << std::endl;
        return;
    }

    // 计算YUV420P文件大小
    long frameSize = static_cast<long>(w) * h * 3 / 2; // Y: w*h, U: w*h/4, V: w*h/4
    std::vector<unsigned char> frameData(frameSize);

    // 读取一帧数据
    inputFile.read(reinterpret_cast<char *>(frameData.data()), frameSize);
    if (inputFile.gcount() != frameSize)
    {
        std::cerr << "Error: Invalid YUV file size" << std::endl;
        inputFile.close();
        return;
    }

    inputFile.close();

    // 分离YUV分量
    int ySize = w * h;
    int uvSize = ySize / 4;

    std::vector<unsigned char> Y(ySize);
    std::vector<unsigned char> U(uvSize);
    std::vector<unsigned char> V(uvSize);

    std::copy(frameData.begin(), frameData.begin() + ySize, Y.begin());
    std::copy(frameData.begin() + ySize, frameData.begin() + ySize + uvSize, U.begin());
    std::copy(frameData.begin() + ySize + uvSize, frameData.end(), V.begin());

    // 生成num张裁剪后的YUV
    for (int n = 1; n <= num; n++)
    {
        // 计算当前裁剪比例
        double currentCropRatio = n * cropRatio;

        // 确保裁剪比例不超过0.5
        if (currentCropRatio > 0.5)
        {
            std::cerr << "Warning: crop ratio for image " << n << " exceeds 0.5, using 0.5" << std::endl;
            currentCropRatio = 0.5;
        }

        // 计算裁剪后的尺寸
        int cropW = static_cast<int>(w * (1.0 - 2 * currentCropRatio));
        int cropH = static_cast<int>(h * (1.0 - 2 * currentCropRatio));

        // 确保尺寸是偶数（YUV420P要求）
        cropW = (cropW / 2) * 2;
        cropH = (cropH / 2) * 2;

        // 确保尺寸大于0
        if (cropW <= 0 || cropH <= 0)
        {
            std::cerr << "Warning: crop size too small for image " << n << ", skipping" << std::endl;
            continue;
        }

        // 计算UV尺寸（YUV420P中UV是Y的一半）
        int cropW_UV = cropW / 2;
        int cropH_UV = cropH / 2;

        // 计算起始位置
        int startX = (w - cropW) / 2;
        int startY = (h - cropH) / 2;
        int startX_UV = startX / 2;
        int startY_UV = startY / 2;

        // 确保起始位置是偶数
        startX = (startX / 2) * 2;
        startY = (startY / 2) * 2;
        startX_UV = startX / 2;
        startY_UV = startY / 2;

        // 创建裁剪后的YUV数据缓冲区
        int cropYSize = cropW * cropH;
        int cropUVSize = cropW_UV * cropH_UV;
        std::vector<unsigned char> cropY(cropYSize);
        std::vector<unsigned char> cropU(cropUVSize);
        std::vector<unsigned char> cropV(cropUVSize);

        // 裁剪Y分量
        for (int yy = 0; yy < cropH; yy++)
        {
            int srcY = startY + yy;
            int dstY = yy;

            for (int xx = 0; xx < cropW; xx++)
            {
                int srcX = startX + xx;
                int dstX = xx;

                cropY[dstY * cropW + dstX] = Y[static_cast<size_t>(srcY) * static_cast<size_t>(w) + static_cast<size_t>(srcX)];
            }
        }

        // 裁剪U分量
        for (int yy = 0; yy < cropH_UV; yy++)
        {
            int srcY = startY_UV + yy;
            int dstY = yy;

            for (int xx = 0; xx < cropW_UV; xx++)
            {
                int srcX = startX_UV + xx;
                int dstX = xx;

                cropU[dstY * cropW_UV + dstX] = U[srcY * (w / 2) + srcX];
            }
        }

        // 裁剪V分量
        for (int yy = 0; yy < cropH_UV; yy++)
        {
            int srcY = startY_UV + yy;
            int dstY = yy;

            for (int xx = 0; xx < cropW_UV; xx++)
            {
                int srcX = startX_UV + xx;
                int dstX = xx;

                cropV[dstY * cropW_UV + dstX] = V[srcY * (w / 2) + srcX];
            }
        }

        // 生成输出文件名（使用 filesystem 拼接）
        fs::path yuvPath = out_path / ("crop" + std::to_string(n) + ".yuv");
        std::ofstream outputFile(yuvPath.string(), std::ios::binary);

        if (!outputFile.is_open())
        {
            std::cerr << "Error: Cannot create output file: " << yuvPath.string() << std::endl;
            continue;
        }

        // 创建全尺寸缓冲区并初始化（先填充默认值）
        std::vector<unsigned char> fullCropY(w * h, 0);       // Y分量，填充0（黑色）
        std::vector<unsigned char> fullCropU(w * h / 4, 128); // U分量，填充128（中性灰）
        std::vector<unsigned char> fullCropV(w * h / 4, 128); // V分量，填充128（中性灰）

        // 将裁剪后的数据复制到中心位置（保留作为回退方案）
        for (int yy = 0; yy < cropH; yy++)
        {
            for (int xx = 0; xx < cropW; xx++)
            {
                fullCropY[(startY + yy) * w + (startX + xx)] = cropY[yy * cropW + xx];
            }
        }

        for (int yy = 0; yy < cropH_UV; yy++)
        {
            for (int xx = 0; xx < cropW_UV; xx++)
            {
                fullCropU[(startY_UV + yy) * (w / 2) + (startX_UV + xx)] = cropU[yy * cropW_UV + xx];
                fullCropV[(startY_UV + yy) * (w / 2) + (startX_UV + xx)] = cropV[yy * cropW_UV + xx];
            }
        }

        // 新增逻辑：将裁剪的中心区域放大（resize）填满整个画幅，替代黑边填充
        bool resized_ok = false;
        try
        {
            if (cropYSize > 0 && cropUVSize > 0)
            {
                // Y 分量 resize 到 full size (h x w)
                cv::Mat cropYMat(cropH, cropW, CV_8UC1, cropY.data());
                cv::Mat resizedYMat;
                cv::resize(cropYMat, resizedYMat, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);

                // UV 分量 resize 到 full UV size (h/2 x w/2)
                cv::Mat cropUMat(cropH_UV, cropW_UV, CV_8UC1, cropU.data());
                cv::Mat resizedUMat;
                cv::resize(cropUMat, resizedUMat, cv::Size(w / 2, h / 2), 0, 0, cv::INTER_LINEAR);

                cv::Mat cropVMat(cropH_UV, cropW_UV, CV_8UC1, cropV.data());
                cv::Mat resizedVMat;
                cv::resize(cropVMat, resizedVMat, cv::Size(w / 2, h / 2), 0, 0, cv::INTER_LINEAR);

                // 检查尺寸并拷贝数据到 full buffer
                if (resizedYMat.total() == static_cast<size_t>(w) * h &&
                    resizedUMat.total() == static_cast<size_t>(w / 2) * (h / 2) &&
                    resizedVMat.total() == static_cast<size_t>(w / 2) * (h / 2))
                {

                    std::memcpy(fullCropY.data(), resizedYMat.data, fullCropY.size());
                    std::memcpy(fullCropU.data(), resizedUMat.data, fullCropU.size());
                    std::memcpy(fullCropV.data(), resizedVMat.data, fullCropV.size());
                    resized_ok = true;
                }
            }
        }
        catch (const cv::Exception &e)
        {
            std::cerr << "Warning: OpenCV exception during resize: " << e.what() << std::endl;
            resized_ok = false;
        }
        catch (...)
        {
            std::cerr << "Warning: Unknown exception during resize" << std::endl;
            resized_ok = false;
        }

        if (!resized_ok)
        {
            // 如果 resize 失败，fullCrop 保持原先中心放置的方式（已在上面完成）
            std::cerr << "Info: Using centered crop (no upscale) for " << yuvPath.string() << std::endl;
        }

        // 写入文件：先Y，再U，再V
        outputFile.write(reinterpret_cast<const char *>(fullCropY.data()), fullCropY.size());
        outputFile.write(reinterpret_cast<const char *>(fullCropU.data()), fullCropU.size());
        outputFile.write(reinterpret_cast<const char *>(fullCropV.data()), fullCropV.size());

        outputFile.close();

        // 同时生成对应的 JPG（将填充后的全尺寸 YUV 拼接为 I420 格式并转换为 BGR 后保存）
        try
        {
            size_t totalSize = static_cast<size_t>(w) * h * 3 / 2;
            std::vector<unsigned char> frameBuf;
            frameBuf.reserve(totalSize);
            // 按 I420 (Y U V) 顺序拼接
            frameBuf.insert(frameBuf.end(), fullCropY.begin(), fullCropY.end());
            frameBuf.insert(frameBuf.end(), fullCropU.begin(), fullCropU.end());
            frameBuf.insert(frameBuf.end(), fullCropV.begin(), fullCropV.end());

            if (frameBuf.size() == totalSize)
            {
                cv::Mat yuvMat(h * 3 / 2, w, CV_8UC1, frameBuf.data());
                cv::Mat bgrMat;
                cv::cvtColor(yuvMat, bgrMat, cv::COLOR_YUV2BGR_I420);

                fs::path jpgPath = out_path / ("crop" + std::to_string(n) + ".jpg");
                if (!cv::imwrite(jpgPath.string(), bgrMat))
                {
                    std::cerr << "Warning: Failed to write JPG: " << jpgPath.string() << std::endl;
                }
            }
            else
            {
                std::cerr << "Warning: frame buffer size mismatch, skip JPG generation for " << yuvPath.string() << std::endl;
            }
        }
        catch (const cv::Exception &e)
        {
            std::cerr << "Warning: OpenCV exception during JPG generation: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "Warning: Unknown error during JPG generation for " << yuvPath.string() << std::endl;
        }

        std::cout << "Generated: " << yuvPath.string()
                  << " (crop ratio: " << currentCropRatio
                  << ", crop size: " << cropW << "x" << cropH
                  << ", upscale applied: " << (resized_ok ? "yes" : "no") << ")" << std::endl;
    }

    std::cout << "Successfully generated " << num << " cropped YUV files." << std::endl;
}

/**
 * @brief 将YUV420P文件转换为OpenCV Mat并显示（用于调试）
 *
 * @param yuvFile YUV文件路径
 * @param w 图像宽度
 * @param h 图像高度
 */
inline void DisplayYuvImage(const std::string &yuvFile, int w, int h)
{
    std::ifstream file(yuvFile, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Cannot open YUV file for display: " << yuvFile << std::endl;
        return;
    }

    int frameSize = w * h * 3 / 2;
    std::vector<unsigned char> data(frameSize);
    file.read(reinterpret_cast<char *>(data.data()), frameSize);
    file.close();

    // 转换为YUV到BGR显示
    cv::Mat yuvMat(h * 3 / 2, w, CV_8UC1, data.data());
    cv::Mat bgrMat;
    cv::cvtColor(yuvMat, bgrMat, cv::COLOR_YUV2BGR_I420);

    cv::imshow(yuvFile, bgrMat);
    cv::waitKey(0);
    cv::destroyAllWindows();
}

// 为封面帧生成不同FOV的过渡帧（最后一帧cropN.yuv FOV与视频帧一致）
inline void GenDiffFovYuvsFromCoverFrame()
{
    auto [x, y] = GetXYScaleFroMatrixVidToCover();                // 获取视频帧到封面帧的X、Y缩放比例
    double vidToCoverScaleRation = (x + y) / 2;                   // 取平均值作为缩放比例
    auto ratio = (1 - vidToCoverScaleRation) / 2 / TransFrameNum; // 0.94是通过视频帧对齐到封面帧得到的H矩阵中的缩放比例，及：视频帧 （size） = 封面帧 94% w * 封面帧 94% h
    PV("GenDiffFovYuvsFromCoverFrame:", x, y, vidToCoverScaleRation, ratio);
    CenterCropGenYuvs(COVER_FRAME_YUV, YUV_W, YUV_H, TransFrameNum, ratio, CROP_COVER_DIR); // crop1 FOV接近封面，cropLast FOV接近视频帧
}

#endif // CENTERCROPGENYUVS_H_
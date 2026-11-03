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

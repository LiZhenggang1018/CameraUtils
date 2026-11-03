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
#include "ImageRegistrator.h"
#include <filesystem>
#include "YUVImageReader.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <objbase.h>
#include <wincodec.h>
#include <windows.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#endif

cv::Ptr<cv::Feature2D> ImageRegistrator::CreateDetector(FeatureDetectorType type) const
{
    const int key = static_cast<int>(type);
    auto it = detectorCache_.find(key);
    if (it != detectorCache_.end()) {
        return it->second;
    }

    cv::Ptr<cv::Feature2D> detector;
    switch (type) {
    case FeatureDetectorType::ORB:
        detector = cv::ORB::create(5000); // 5000 个点，保证 RANSAC 有足够内点
        break;
    case FeatureDetectorType::SIFT: //
        detector = cv::SIFT::create(5000);
        break;
    case FeatureDetectorType::BRISK:
        detector = cv::BRISK::create();
        break;
    case FeatureDetectorType::KAZE:
        detector = cv::KAZE::create();
        break;
    case FeatureDetectorType::AKAZE:
        detector = cv::AKAZE::create();
        break;
    default:
        detector = cv::ORB::create(5000);
        break;
    }

    detectorCache_[key] = detector;
    return detector;
}

std::tuple<std::vector<cv::KeyPoint>, cv::Mat> ImageRegistrator::DetectFeatures(const cv::Mat& image,
                                                                                FeatureDetectorType detectorType)
{
    // 修复坑 6（兼容性）：支持 BGR / BGRA / GRAY 三种输入
    cv::Mat gray;
    if (image.channels() == 1) {
        gray = image;
    } else if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    } else {
        throw std::runtime_error("不支持的图像通道数: " + std::to_string(image.channels()));
    }

    auto detector = CreateDetector(detectorType); // 缓存命中
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;

    detector->detectAndCompute(gray, cv::noArray(), keypoints, descriptors);

    return {keypoints, descriptors};
}

std::vector<cv::DMatch> ImageRegistrator::MatchFeatures(const cv::Mat& descriptors1, const cv::Mat& descriptors2,
                                                        FeatureDetectorType detectorType)
{
    if (descriptors1.empty() || descriptors2.empty()) {
        return {};
    }

    cv::Ptr<cv::DescriptorMatcher> matcher;
    bool useFlann = false;

    // ORB/BRISK/AKAZE 默认二值描述符 → NORM_HAMMING
    // KAZE 是浮点描述符 → NORM_L2
    if (detectorType == FeatureDetectorType::ORB || detectorType == FeatureDetectorType::BRISK ||
        detectorType == FeatureDetectorType::AKAZE) {
        matcher = cv::BFMatcher::create(cv::NORM_HAMMING);
        useFlann = false;
    } else {
        matcher = cv::BFMatcher::create(cv::NORM_L2);
        useFlann = true;
    }

    // 浮点描述符尝试 FLANN（快 2~3 倍），失败回退 BFMatcher
    if (useFlann && !descriptors1.empty() && !descriptors2.empty()) {
        try {
            cv::Ptr<cv::flann::IndexParams> indexParams;
            if (descriptors1.type() == CV_32F) {
                indexParams = cv::makePtr<cv::flann::KDTreeIndexParams>(5);
            } else {
                indexParams = cv::makePtr<cv::flann::LshIndexParams>(20, 10, 2);
            }

            cv::Ptr<cv::flann::SearchParams> searchParams = cv::makePtr<cv::flann::SearchParams>(50);
            matcher = cv::makePtr<cv::FlannBasedMatcher>(indexParams, searchParams);
        } catch (...) {
            matcher = cv::BFMatcher::create(cv::NORM_L2); // 回退
        }
    }

    // KNN + Lowe's ratio test（0.75 是经典阈值）
    std::vector<std::vector<cv::DMatch>> knnMatches;
    matcher->knnMatch(descriptors1, descriptors2, knnMatches, 2);

    std::vector<cv::DMatch> goodMatches;
    goodMatches.reserve(knnMatches.size());

    const float ratioThresh = 0.75F;
    for (const auto& knnMatchItem : knnMatches) {
        if (knnMatchItem.size() == 2) {
            if (knnMatchItem[0].distance < ratioThresh * knnMatchItem[1].distance) {
                goodMatches.push_back(knnMatchItem[0]);
            }
        }
    }

    return goodMatches;
}

cv::Mat ImageRegistrator::EstimateTransform(const std::vector<cv::KeyPoint>& keypoints1,
                                            const std::vector<cv::KeyPoint>& keypoints2,
                                            const std::vector<cv::DMatch>& matches, TransformType transformType,
                                            std::vector<cv::DMatch>& inlierMatches)
{
    if (matches.size() < 4) {
        throw std::runtime_error("匹配点不足4个，无法估计变换矩阵");
    }

    // 提取匹配点坐标（Point2f，720×1584 下足够精度）
    std::vector<cv::Point2f> points1;
    std::vector<cv::Point2f> points2;
    points1.reserve(matches.size());
    points2.reserve(matches.size());
    for (const auto& match : matches) {
        points1.push_back(keypoints1[match.queryIdx].pt);
        points2.push_back(keypoints2[match.trainIdx].pt);
    }

    cv::Mat transform;
    std::vector<uchar> inliersMask;

    try {
        switch (transformType) {
        case TransformType::HOMOGRAPHY:
            transform = cv::findHomography(points1, points2, cv::RANSAC, 3.0, inliersMask, 2000, 0.995);
            break;

        case TransformType::AFFINE:
            transform = cv::estimateAffine2D(points1, points2, inliersMask, cv::RANSAC, 3.0, 2000, 0.995);
            if (!transform.empty()) {
                cv::Mat h(3, 3, CV_64F, cv::Scalar(0));
                transform.copyTo(h.rowRange(0, 2));
                h.at<double>(2, 2) = 1.0;
                transform = h;
            }
            break;

        case TransformType::EUCLIDEAN: {
            cv::Mat rigidTransform = cv::estimateAffinePartial2D(points1, points2, inliersMask, cv::RANSAC, 3.0);
            if (!rigidTransform.empty()) {
                cv::Mat h(3, 3, CV_64F, cv::Scalar(0));
                rigidTransform.copyTo(h.rowRange(0, 2));
                h.at<double>(2, 2) = 1.0;
                transform = h;
            }
            break;
        }

        case TransformType::TRANSLATION: {
            // 中位数平移，不做 RANSAC，所有匹配都算内点
            std::vector<float> dx;
            std::vector<float> dy;
            dx.reserve(points1.size());
            dy.reserve(points1.size());
            for (size_t i = 0; i < points1.size(); ++i) {
                dx.push_back(points2[i].x - points1[i].x);
                dy.push_back(points2[i].y - points1[i].y);
            }
            auto mid = dx.size() / 2;
            std::nth_element(dx.begin(), dx.begin() + mid, dx.end());
            std::nth_element(dy.begin(), dy.begin() + mid, dy.end());
            float tx = dx[mid];
            float ty = dy[mid];

            transform = (cv::Mat_<double>(3, 3) << 1, 0, tx, 0, 1, ty, 0, 0, 1);
            inliersMask.assign(points1.size(), 1);
            break;
        }
        }
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV异常: " << e.what() << std::endl;
        throw std::runtime_error("估计变换矩阵失败");
    }

    // 修复（之前 #10）：inliers_mask 空检查，避免越界
    if (transform.empty() || inliersMask.size() != matches.size()) {
        std::cerr << "Warning: EstimateTransform failed or inliers_mask size mismatch"
                  << " (matches=" << matches.size() << ", mask=" << inliersMask.size() << ")" << std::endl;
        inlierMatches.clear();
        return cv::Mat();
    }

    // 提取内点匹配（修复坑 4：现在通过引用传回，最终存进 RegistrationResult::inlier_matches）
    inlierMatches.clear();
    inlierMatches.reserve(matches.size());
    for (size_t i = 0; i < matches.size(); ++i) {
        if (inliersMask[i]) {
            inlierMatches.push_back(matches[i]);
        }
    }

    return transform;
}

cv::Mat ImageRegistrator::ApplyTransform(const cv::Mat& image, const cv::Mat& transform, const cv::Size& outputSize)
{
    if (transform.empty()) {
        throw std::runtime_error("变换矩阵为空");
    }

    cv::Mat registeredImage;

    if (transform.rows == 3 && transform.cols == 3) {
        cv::warpPerspective(image, registeredImage, transform, outputSize, cv::INTER_LINEAR);
    } else if (transform.rows == 2 && transform.cols == 3) {
        cv::warpAffine(image, registeredImage, transform, outputSize, cv::INTER_LINEAR);
    } else {
        throw std::runtime_error("不支持的变换矩阵维度");
    }

    return registeredImage;
}

RegistrationResult ImageRegistrator::RegisterImages(const cv::Mat& referenceImg, const cv::Mat& targetImg,
                                                    FeatureDetectorType detectorType, TransformType transformType)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    RegistrationResult result;

    // 1. 检测特征点
    std::cout << "检测特征点..." << std::endl;
    auto [keypoints1, descriptors1] = DetectFeatures(referenceImg, detectorType);
    auto [keypoints2, descriptors2] = DetectFeatures(targetImg, detectorType);

    std::cout << "参考图像特征点数: " << keypoints1.size() << std::endl;
    std::cout << "目标图像特征点数: " << keypoints2.size() << std::endl;

    // 2. 匹配特征点
    std::cout << "匹配特征点..." << std::endl;
    result.goodMatches = MatchFeatures(descriptors1, descriptors2, detectorType);
    std::cout << "匹配点数: " << result.goodMatches.size() << std::endl;

    // 3. 估计变换矩阵（修复坑 4：inlier_matches 存进 result）
    std::cout << "估计变换矩阵..." << std::endl;
    result.transformMatrix =
        EstimateTransform(keypoints1, keypoints2, result.goodMatches, transformType, result.inlierMatches);

    result.numInliers = static_cast<int>(result.inlierMatches.size());
    std::cout << "内点数: " << result.numInliers << std::endl;

    // ⚠️ 重要约定（修复坑 1）：
    //   transform_matrix 是 reference → target（points1 → points2）
    //   要把 target 对齐到 reference，必须求逆后再 warp
    cv::Mat warpToRef;
    if (!result.transformMatrix.empty()) {
        if (result.transformMatrix.rows == 3 && result.transformMatrix.cols == 3) {
            cv::Mat inv;
            if (!cv::invert(result.transformMatrix, inv, cv::DECOMP_SVD)) {
                throw std::runtime_error("无法求逆变换矩阵");
            }
            warpToRef = inv;
        } else if (result.transformMatrix.rows == 2 && result.transformMatrix.cols == 3) {
            cv::Mat h(3, 3, CV_64F, cv::Scalar(0));
            result.transformMatrix.copyTo(h.rowRange(0, 2));
            h.at<double>(2, 2) = 1.0;
            cv::Mat inv;
            if (!cv::invert(h, inv, cv::DECOMP_SVD)) {
                throw std::runtime_error("无法求逆仿射矩阵");
            }
            warpToRef = inv;
        } else {
            throw std::runtime_error("不支持的变换矩阵维度");
        }
    } else {
        throw std::runtime_error("估计出的变换矩阵为空");
    }

    // 4. 应用变换
    std::cout << "应用变换..." << std::endl;
    result.registeredImage = ApplyTransform(targetImg, warpToRef, referenceImg.size());

    // 5. RMS 误差（修复坑 2：内部已说明是"内点重投影误差"，不是"整图像素差"）
    result.rmsError = 0.0;
    if (!result.inlierMatches.empty() && !warpToRef.empty()) {
        for (const auto& match : result.inlierMatches) {
            cv::Point2f ptRef = keypoints1[match.queryIdx].pt;
            cv::Point2f ptTar = keypoints2[match.trainIdx].pt;

            cv::Mat ptTarHomog = (cv::Mat_<double>(3, 1) << ptTar.x, ptTar.y, 1);
            cv::Mat ptTarTransformed = warpToRef * ptTarHomog;
            ptTarTransformed /= ptTarTransformed.at<double>(2, 0);

            double dx = ptRef.x - ptTarTransformed.at<double>(0, 0);
            double dy = ptRef.y - ptTarTransformed.at<double>(1, 0);
            result.rmsError += dx * dx + dy * dy;
        }
        result.rmsError = std::sqrt(result.rmsError / result.inlierMatches.size());
    }

    result.keypoints1 = std::move(keypoints1);
    result.keypoints2 = std::move(keypoints2);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "配准完成，耗时: " << duration.count() << "ms" << std::endl;
    std::cout << "RMS误差: " << result.rmsError << " 像素" << std::endl;

    return result;
}

cv::Mat ImageRegistrator::DrawMatches(const cv::Mat& img1, const cv::Mat& img2,
                                      const std::vector<cv::KeyPoint>& keypoints1,
                                      const std::vector<cv::KeyPoint>& keypoints2,
                                      const std::vector<cv::DMatch>& matches)
{
    cv::Mat matchesImg;
    cv::drawMatches(img1, keypoints1, img2, keypoints2, matches, matchesImg, cv::Scalar::all(-1), cv::Scalar::all(-1),
                    std::vector<char>(), cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
    return matchesImg;
}

void ImageRegistrator::SaveRegistrationResults(const RegistrationResult& result, const std::string& outputDir)
{
    std::filesystem::create_directories(outputDir);

    // 1. 保存配准后的图像
    {
        std::string outputPath = outputDir + "/registered_image.jpg";
        cv::imwrite(outputPath, result.registeredImage);
    }

    // 2. 保存变换矩阵
    {
        std::string transformPath = outputDir + "/transform_matrix.txt";
        std::ofstream transformFile(transformPath);
        if (transformFile.is_open()) {
            transformFile << "变换矩阵 (reference -> target):\n";
            transformFile << result.transformMatrix << std::endl;
            transformFile << "\n配准参数:\n";
            transformFile << "内点数: " << result.numInliers << std::endl;
            transformFile << "RMS误差: " << result.rmsError << " 像素" << std::endl;
            transformFile.close();
        }
    }

    // 3. 保存 good_matches（兼容旧工具）
    {
        std::string matchesPath = outputDir + "/matches.txt";
        std::ofstream matchesFile(matchesPath);
        if (matchesFile.is_open()) {
            matchesFile << "匹配点数量: " << result.goodMatches.size() << std::endl;
            matchesFile << "\n匹配点详细信息:\n";
            matchesFile << "索引, 参考图像X, 参考图像Y, 目标图像X, 目标图像Y, 距离\n";

            for (size_t i = 0; i < result.goodMatches.size(); ++i) {
                const auto& match = result.goodMatches[i];
                cv::Point2f pt1 = result.keypoints1[match.queryIdx].pt;
                cv::Point2f pt2 = result.keypoints2[match.trainIdx].pt;

                matchesFile << i << ", " << pt1.x << ", " << pt1.y << ", " << pt2.x << ", " << pt2.y << ", "
                            << match.distance << std::endl;
            }
            matchesFile.close();
        }
    }

    // 4. 保存 inlier_matches（修复坑 4：RANSAC 内点，更可信）
    {
        std::string inliersPath = outputDir + "/inlier_matches.txt";
        std::ofstream inliersFile(inliersPath);
        if (inliersFile.is_open()) {
            inliersFile << "RANSAC 内点数量: " << result.inlierMatches.size() << std::endl;
            inliersFile << "\n内点详细信息:\n";
            inliersFile << "索引, 参考图像X, 参考图像Y, 目标图像X, 目标图像Y, 距离\n";

            for (size_t i = 0; i < result.inlierMatches.size(); ++i) {
                const auto& match = result.inlierMatches[i];
                cv::Point2f pt1 = result.keypoints1[match.queryIdx].pt;
                cv::Point2f pt2 = result.keypoints2[match.trainIdx].pt;

                inliersFile << i << ", " << pt1.x << ", " << pt1.y << ", " << pt2.x << ", " << pt2.y << ", "
                            << match.distance << std::endl;
            }
            inliersFile.close();
        }
    }
}

// ============================================================
// 便捷配准入口实现
// ============================================================

namespace {

#ifdef _WIN32
// Windows 原生图像解码（支持 heif/heic/avif 等系统解码器支持的格式）
cv::Mat LoadImageWithWIC(const std::string& path)
{
    // UTF-8 → UTF-16
    std::wstring wpath;
    {
        const int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        if (len <= 0) {
            return {};
        }
        wpath.resize(static_cast<size_t>(len));
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), len);
        if (!wpath.empty() && wpath.back() == L'\0') {
            wpath.pop_back();
        }
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool needUninit = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return {};
    }

    cv::Mat result;
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    do {
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
        if (FAILED(hr)) {
            break;
        }

        hr = factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                                &decoder);
        if (FAILED(hr)) {
            break;
        }

        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) {
            break;
        }

        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr)) {
            break;
        }

        hr = converter->Initialize(frame, GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) {
            break;
        }

        UINT w = 0;
        UINT h = 0;
        hr = converter->GetSize(&w, &h);
        if (FAILED(hr) || w == 0 || h == 0) {
            break;
        }

        const UINT stride = w * 3;
        const UINT bufSize = stride * h;
        std::vector<uint8_t> buf(bufSize);

        hr = converter->CopyPixels(nullptr, stride, bufSize, buf.data());
        if (FAILED(hr)) {
            break;
        }

        cv::Mat tmp(static_cast<int>(h), static_cast<int>(w), CV_8UC3, buf.data());
        result = tmp.clone();
    } while (false);

    if (converter) {
        converter->Release();
    }
    if (frame) {
        frame->Release();
    }
    if (decoder) {
        decoder->Release();
    }
    if (factory) {
        factory->Release();
    }
    if (needUninit) {
        CoUninitialize();
    }

    return result;
}
#endif // _WIN32

// 自动加载图像：优先 cv::imread，失败则降级到 WIC
cv::Mat LoadImageAuto(const std::string& path)
{
    cv::Mat img = cv::imread(path, cv::IMREAD_COLOR);
    if (!img.empty()) {
        std::cout << "[LoadImageAuto] cv::imread OK: " << path << " (" << img.cols << "x" << img.rows << ")"
                  << std::endl;
        return img;
    }

#ifdef _WIN32
    img = LoadImageWithWIC(path);
    if (!img.empty()) {
        std::cout << "[LoadImageAuto] WIC OK: " << path << " (" << img.cols << "x" << img.rows << ")" << std::endl;
        return img;
    }
#endif

    std::cerr << "[LoadImageAuto] Failed to load image: " << path << " (neither imread nor WIC succeeded)" << std::endl;
    return {};
}

// 共用逻辑：对两张已加载的 BGR 图做配准，保存结果
// 成功返回 registered_image，失败返回空 Mat
cv::Mat RegFromMats(const cv::Mat& targetImg, const cv::Mat& referenceImg, const std::string& outputDir,
                    FeatureDetectorType detector, TransformType transform)
{
    try {
        std::filesystem::create_directories(outputDir);

        // 01 / 02：保存原始图
        if (!cv::imwrite(outputDir + "/01_reference_original.jpg", referenceImg)) {
            std::cerr << "Warning: cannot write 01_reference_original.jpg" << std::endl;
        }
        if (!cv::imwrite(outputDir + "/02_target_original.jpg", targetImg)) {
            std::cerr << "Warning: cannot write 02_target_original.jpg" << std::endl;
        }

        ImageRegistrator registrator;
        auto result = registrator.RegisterImages(referenceImg, targetImg, detector, transform);

        if (result.registeredImage.empty() || result.transformMatrix.empty()) {
            std::cerr << "Registration failed: empty registered_image or transform_matrix" << std::endl;
            return {};
        }

        // 基础产物：registered_image.jpg / transform_matrix.txt / matches.txt / inlier_matches.txt
        registrator.SaveRegistrationResults(result, outputDir);

        // 03：特征匹配
        cv::Mat matchesImg =
            registrator.DrawMatches(referenceImg, targetImg, result.keypoints1, result.keypoints2, result.goodMatches);
        cv::imwrite(outputDir + "/03_feature_matches.jpg", matchesImg);

        // 04：半透明叠加
        cv::Mat overlay;
        cv::addWeighted(referenceImg, 0.5, result.registeredImage, 0.5, 0.0, overlay);
        cv::imwrite(outputDir + "/04_overlay_result.jpg", overlay);

        // 05：并排
        cv::Mat sideBySide;
        cv::hconcat(referenceImg, result.registeredImage, sideBySide);
        cv::imwrite(outputDir + "/05_side_by_side.jpg", sideBySide);

        // 06：差异
        cv::Mat diff;
        cv::absdiff(referenceImg, result.registeredImage, diff);
        cv::imwrite(outputDir + "/06_difference.jpg", diff);

        std::cout << "[RegFromMats] done. inliers=" << result.numInliers << ", rms=" << result.rmsError << " px"
                  << std::endl;
        std::cout << "[RegFromMats] output: " << outputDir << std::endl;

        return result.registeredImage;
    } catch (const std::exception& e) {
        std::cerr << "[RegFromMats] error: " << e.what() << std::endl;
        return {};
    }
}

} // namespace

int RegYuv(const std::string& curYuvFile, const std::string& refYuvFile, const std::string& outputDir, int width,
           int height, FeatureDetectorType detector, TransformType transform)
{
    try {
        YUVImageReader reader(width, height);

        cv::Mat refImg = reader.ReadYuv420P(refYuvFile);
        if (refImg.empty()) {
            std::cerr << "Failed to read reference YUV: " << refYuvFile << std::endl;
            return 1;
        }

        cv::Mat curImg = reader.ReadYuv420P(curYuvFile);
        if (curImg.empty()) {
            std::cerr << "Failed to read target YUV: " << curYuvFile << std::endl;
            return 2;
        }

        std::cout << "[RegYuv] ref=" << refYuvFile << " (" << refImg.cols << "x" << refImg.rows << ")" << std::endl;
        std::cout << "[RegYuv] cur=" << curYuvFile << " (" << curImg.cols << "x" << curImg.rows << ")" << std::endl;

        cv::Mat registered = RegFromMats(curImg, refImg, outputDir, detector, transform);
        if (registered.empty()) {
            return 3;
        }

        // 07：配准后的 YUV（RegYuv 特有）
        reader.SaveAsYuv420P(registered, outputDir + "/07_registered_image.yuv");
        std::cout << "[RegYuv] saved 07_registered_image.yuv" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[RegYuv] error: " << e.what() << std::endl;
        return 4;
    }
}

int RegImg(const std::string& curImgFile, const std::string& refImgFile, const std::string& outputDir,
           FeatureDetectorType detector, TransformType transform)
{
    try {
        cv::Mat refImg = LoadImageAuto(refImgFile);
        if (refImg.empty()) {
            std::cerr << "Failed to load reference image: " << refImgFile << std::endl;
            return 1;
        }

        cv::Mat curImg = LoadImageAuto(curImgFile);
        if (curImg.empty()) {
            std::cerr << "Failed to load target image: " << curImgFile << std::endl;
            return 2;
        }

        std::cout << "[RegImg] ref=" << refImgFile << " (" << refImg.cols << "x" << refImg.rows << ")" << std::endl;
        std::cout << "[RegImg] cur=" << curImgFile << " (" << curImg.cols << "x" << curImg.rows << ")" << std::endl;

        cv::Mat registered = RegFromMats(curImg, refImg, outputDir, detector, transform);
        if (registered.empty()) {
            return 3;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[RegImg] error: " << e.what() << std::endl;
        return 4;
    }
}
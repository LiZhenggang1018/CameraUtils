#ifndef INC_ALIGNMENT_PANORAMA_H
#define INC_ALIGNMENT_PANORAMA_H

#include <vector>

#include "OpenCVCommon.h"

#include "Alignment/Blending.h"
#include "Alignment/ExposureCompensation.h"
#include "Alignment/Homography.h"

// 多图全景拼接核心。
// 定位：组合第 8 章模块（曝光补偿 + 配准累计单应 + warp + 多波段融合），实现横向多图全景。
// 流程：曝光补偿 → 两两配准累计单应 → 计算总画布 → warp 各图 → 多波段融合。
namespace Alignment {

inline cv::Mat StitchPanorama(const std::vector<cv::Mat>& images)
{
    if (images.size() < 2) {
        return images.empty() ? cv::Mat() : images[0].clone();
    }
    for (const auto& img : images) {
        if (img.empty()) {
            return {};
        }
    }

    // 1. 曝光补偿
    const auto gains = EstimateGains(images);
    std::vector<cv::Mat> comp;
    comp.reserve(images.size());
    for (std::size_t i = 0; i < images.size(); ++i) {
        comp.push_back(ApplyGain(images[i], gains[i]));
    }

    // 2. 累计单应：以中间图为参考（减少累计误差）
    const int refIdx = static_cast<int>(images.size()) / 2;
    std::vector<cv::Mat> homographies(images.size(), cv::Mat::eye(3, 3, CV_64F));

    // 参考图左侧的图：向右累计
    for (int i = refIdx - 1; i >= 0; --i) {
        std::vector<cv::DMatch> inliers;
        // img[i] -> img[i+1] 的单应
        cv::Mat h = ComputeHomography(comp[i], comp[i + 1], inliers);
        if (h.empty()) {
            h = cv::Mat::eye(3, 3, CV_64F);
        }
        homographies[i] = homographies[i + 1] * h;
    }
    // 参考图右侧的图：向左累计
    for (int i = refIdx + 1; i < static_cast<int>(images.size()); ++i) {
        std::vector<cv::DMatch> inliers;
        // img[i] -> img[i-1] 的单应
        cv::Mat h = ComputeHomography(comp[i], comp[i - 1], inliers);
        if (h.empty()) {
            h = cv::Mat::eye(3, 3, CV_64F);
        }
        homographies[i] = homographies[i - 1] * h;
    }

    // 3. 计算总画布（各图 warp 到参考坐标系后求并集边界）
    const cv::Mat refImg = comp[refIdx];
    double minX = 0.0;
    double minY = 0.0;
    double maxX = refImg.cols;
    double maxY = refImg.rows;
    for (std::size_t i = 0; i < images.size(); ++i) {
        const cv::Mat h = homographies[i];
        const std::vector<cv::Point2f> corners = {
            cv::Point2f(0, 0), cv::Point2f(static_cast<float>(comp[i].cols), 0),
            cv::Point2f(static_cast<float>(comp[i].cols), static_cast<float>(comp[i].rows)),
            cv::Point2f(0, static_cast<float>(comp[i].rows))};
        std::vector<cv::Point2f> warpedCorners;
        cv::perspectiveTransform(corners, warpedCorners, h);
        for (const auto& p : warpedCorners) {
            minX = std::min(minX, static_cast<double>(p.x));
            minY = std::min(minY, static_cast<double>(p.y));
            maxX = std::max(maxX, static_cast<double>(p.x));
            maxY = std::max(maxY, static_cast<double>(p.y));
        }
    }

    const int canvasW = static_cast<int>(std::ceil(maxX - minX));
    const int canvasH = static_cast<int>(std::ceil(maxY - minY));
    if (canvasW <= 0 || canvasH <= 0) {
        return refImg.clone();
    }

    // 平移单应（把画布左上角移到原点）
    cv::Mat translate = cv::Mat::eye(3, 3, CV_64F);
    translate.at<double>(0, 2) = -minX;
    translate.at<double>(1, 2) = -minY;

    // 4. warp 各图到统一画布 + 计算各图 mask
    std::vector<cv::Mat> warped(images.size());
    std::vector<cv::Mat> masks(images.size());
    for (std::size_t i = 0; i < images.size(); ++i) {
        cv::Mat hFinal = translate * homographies[i];
        warped[i] = WarpImage(comp[i], hFinal, cv::Size(canvasW, canvasH));
        cv::Mat mask(comp[i].size(), CV_8UC1, cv::Scalar(255));
        masks[i] = WarpImage(mask, hFinal, cv::Size(canvasW, canvasH));
    }

    // 5. 顺序融合（两两多波段融合）
    cv::Mat result = warped[0].clone();
    cv::Mat resultMask = masks[0].clone();
    for (std::size_t i = 1; i < warped.size(); ++i) {
        // mask 转 float 供融合
        cv::Mat maskA;
        cv::Mat maskB;
        resultMask.convertTo(maskA, CV_8U);
        masks[i].convertTo(maskB, CV_8U);

        // 归一化 mask（重叠区权重和=1）
        cv::Mat maskAf;
        cv::Mat maskBf;
        maskA.convertTo(maskAf, CV_32F);
        maskB.convertTo(maskBf, CV_32F);
        cv::Mat sum;
        cv::add(maskAf, maskBf, sum);
        sum.setTo(1.0, sum == 0);
        cv::divide(maskAf, sum, maskAf);
        cv::divide(maskBf, sum, maskBf);

        cv::Mat m1;
        cv::Mat m2;
        maskAf.convertTo(m1, CV_8U, 255.0);
        maskBf.convertTo(m2, CV_8U, 255.0);

        result = MultiBandBlend(result, warped[i], m1, m2, 4);
        cv::bitwise_or(resultMask, masks[i], resultMask);
    }

    return result;
}

} // namespace Alignment

#endif // INC_ALIGNMENT_PANORAMA_H

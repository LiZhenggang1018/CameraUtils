#include "FeatureMatching/SiftDetector.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace FeatureMatching {
namespace {

// ---------------------------------------------------------------------------
// 常量与数值工具
// ---------------------------------------------------------------------------

// 描述子结构：16x16 邻域 -> 4x4 子块 -> 每块 8 方向直方图 = 128 维。
constexpr int kDescriptorHistWidth = 4;
constexpr int kDescriptorHistBins = 8;
constexpr int kDescriptorSize = kDescriptorHistWidth * kDescriptorHistWidth * kDescriptorHistBins;

// 方向直方图：36 bin（每 10 度一个），峰值 80% 以上的为辅方向。
constexpr int kOrientationBins = 36;
constexpr double kOrientationPeakRatio = 0.8;

// 描述子归一化：L2 归一化后截断到 0.2 再归一化（抗非线性光照）。
constexpr float kDescriptorClip = 0.2f;

constexpr double kPi = 3.14159265358979323846;

double NormalizeAngle(double rad)
{
    while (rad < 0.0) {
        rad += 2.0 * kPi;
    }
    while (rad >= 2.0 * kPi) {
        rad -= 2.0 * kPi;
    }
    return rad;
}

// ---------------------------------------------------------------------------
// 阶段 1：DoG 尺度空间构建
//
// 高斯金字塔：[octave][layer] 的 CV_32F 灰度图，octave 间 2 倍降采样，octave 内
// sigma 按 k = 2^(1/nOctaveLayers) 递增。DoG = 相邻高斯层之差（有正有负，须浮点）。
// ---------------------------------------------------------------------------

struct ScaleSpace {
    std::vector<std::vector<cv::Mat>> gaussian; // [octave][layer]（CV_32F）
    std::vector<std::vector<cv::Mat>> dog;      // [octave][layer-1]（相邻差）
};

ScaleSpace BuildScaleSpace(const cv::Mat& gray, int nOctaveLayers, double sigma)
{
    const int extraLayers = 3; // 首尾各多算 1 层用于极值搜索，octave 间降采样取倒数第 3 层
    const int layersPerOctave = nOctaveLayers + extraLayers;

    // octave 数：一路 2 倍降采样，直到短边小于 2 像素无法继续。
    int nOctaves = 0;
    int rows = gray.rows;
    int cols = gray.cols;
    while (rows >= 2 && cols >= 2) {
        ++nOctaves;
        rows = (rows + 1) / 2;
        cols = (cols + 1) / 2;
    }

    const double k = std::pow(2.0, 1.0 / nOctaveLayers);

    // 各 octave 各层的 sigma（octave-local 单位，即相对本 octave 第 0 层已平滑到 sigma）。
    // 相邻层之间需要增量平滑 sigma_step = sqrt(total^2 - prev^2)。
    ScaleSpace ss;
    ss.gaussian.resize(nOctaves);
    ss.dog.resize(nOctaves);

    // 输入转 float（DoG 是相邻高斯之差，有正有负，须用浮点存储）。
    cv::Mat grayF;
    gray.convertTo(grayF, CV_32F);

    for (int o = 0; o < nOctaves; ++o) {
        // 本 octave 的基图：octave 0 用原图（先做一次 sigma 预平滑），后续 octave
        // 用上一 octave 倒数第 3 层降采样。
        cv::Mat base;
        if (o == 0) {
            cv::GaussianBlur(grayF, base, cv::Size(0, 0), sigma);
        } else {
            cv::resize(ss.gaussian[o - 1][nOctaveLayers], base, cv::Size(), 0.5, 0.5, cv::INTER_LINEAR);
        }

        ss.gaussian[o].resize(layersPerOctave);
        cv::Mat cur = base;
        for (int s = 0; s < layersPerOctave; ++s) {
            if (s == 0) {
                ss.gaussian[o][s] = cur.clone();
            } else {
                // 从上一层的 sigma 累计到本层：s 层目标 sigma = sigma * k^s。
                const double prevSigma = sigma * std::pow(k, s - 1);
                const double totalSigma = sigma * std::pow(k, s);
                const double step = std::sqrt(std::max(0.0, totalSigma * totalSigma - prevSigma * prevSigma));
                cv::Mat blurred;
                if (step > 0.0) {
                    cv::GaussianBlur(cur, blurred, cv::Size(0, 0), step);
                } else {
                    blurred = cur.clone();
                }
                ss.gaussian[o][s] = blurred;
                cur = blurred;
            }
        }

        // DoG：相邻高斯层相减。
        ss.dog[o].resize(layersPerOctave - 1);
        for (int s = 0; s < layersPerOctave - 1; ++s) {
            cv::subtract(ss.gaussian[o][s + 1], ss.gaussian[o][s], ss.dog[o][s]);
        }
    }

    return ss;
}

// ---------------------------------------------------------------------------
// 阶段 2：极值检测 + 剔除
// ---------------------------------------------------------------------------

// 在 DoG 3x3x3 邻域检测极值（严格大于/小于 26 个邻居）。
bool IsExtremum(const std::vector<std::vector<cv::Mat>>& dog, int octave, int layer, int y, int x)
{
    const double val = dog[octave][layer].at<float>(y, x);
    bool isMax = true;
    bool isMin = true;
    for (int dy = -1; dy <= 1 && (isMax || isMin); ++dy) {
        for (int dx = -1; dx <= 1 && (isMax || isMin); ++dx) {
            if (dy == 0 && dx == 0) {
                continue;
            }
            const double n = dog[octave][layer].at<float>(y + dy, x + dx);
            if (n >= val) {
                isMax = false;
            }
            if (n <= val) {
                isMin = false;
            }
        }
    }
    if (!isMax && !isMin) {
        return false;
    }
    // 相邻尺度层也要满足（若存在）。
    for (int dl = -1; dl <= 1; dl += 2) {
        const int nl = layer + dl;
        if (nl < 0 || nl >= static_cast<int>(dog[octave].size())) {
            continue;
        }
        const double n = dog[octave][nl].at<float>(y, x);
        if (isMax && n >= val) {
            return false;
        }
        if (isMin && n <= val) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// 阶段 2.5：亚像素关键点定位（Taylor 展开）
// ---------------------------------------------------------------------------

// 3x3 行列式。
double Det3(double a00, double a01, double a02, double a10, double a11, double a12, double a20, double a21, double a22)
{
    return a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) + a02 * (a10 * a21 - a11 * a20);
}

// 用 DoG 的三元二次 Taylor 展开把整数极值 (y, x, layer) 精化到亚像素精度。
// 成功时输出精化坐标 (refinedY, refinedX)、尺度偏移 layerOffset 与精化后的对比度 refinedResponse；
// 偏移任一维超过 0.5、Hessian 奇异或对比度过低时返回 false（该极值不稳定，剔除）。
bool RefineExtremum(const std::vector<std::vector<cv::Mat>>& dog, int octave, int layer, int y, int x,
                    double contrastThreshold, double& refinedY, double& refinedX, double& layerOffset,
                    double& refinedResponse)
{
    const cv::Mat& d0 = dog[octave][layer - 1];
    const cv::Mat& d1 = dog[octave][layer];
    const cv::Mat& d2 = dog[octave][layer + 1];

    const double v = d1.at<float>(y, x);

    // 一阶导（中心差分）。
    const double gx = (static_cast<double>(d1.at<float>(y, x + 1)) - d1.at<float>(y, x - 1)) / 2.0;
    const double gy = (static_cast<double>(d1.at<float>(y + 1, x)) - d1.at<float>(y - 1, x)) / 2.0;
    const double gs = (static_cast<double>(d2.at<float>(y, x)) - d0.at<float>(y, x)) / 2.0;

    // Hessian（3x3 对称）。
    const double hxx = static_cast<double>(d1.at<float>(y, x + 1)) - 2.0 * v + d1.at<float>(y, x - 1);
    const double hyy = static_cast<double>(d1.at<float>(y + 1, x)) - 2.0 * v + d1.at<float>(y - 1, x);
    const double hss = static_cast<double>(d2.at<float>(y, x)) - 2.0 * v + d0.at<float>(y, x);
    const double hxy = (static_cast<double>(d1.at<float>(y + 1, x + 1)) - d1.at<float>(y + 1, x - 1) -
                        d1.at<float>(y - 1, x + 1) + d1.at<float>(y - 1, x - 1)) /
                       4.0;
    const double hxs = (static_cast<double>(d2.at<float>(y, x + 1)) - d2.at<float>(y, x - 1) - d0.at<float>(y, x + 1) +
                        d0.at<float>(y, x - 1)) /
                       4.0;
    const double hys = (static_cast<double>(d2.at<float>(y + 1, x)) - d2.at<float>(y - 1, x) - d0.at<float>(y + 1, x) +
                        d0.at<float>(y - 1, x)) /
                       4.0;

    // 解 H * offset = -g（克拉默法则）。
    const double detH = Det3(hxx, hxy, hxs, hxy, hyy, hys, hxs, hys, hss);
    if (std::abs(detH) < 1e-12) {
        return false;
    }
    const double detX = Det3(-gx, hxy, hxs, -gy, hyy, hys, -gs, hys, hss);
    const double detY = Det3(hxx, -gx, hxs, hxy, -gy, hys, hxs, -gs, hss);
    const double detS = Det3(hxx, hxy, -gx, hxy, hyy, -gy, hxs, hys, -gs);

    const double dx = detX / detH;
    const double dy = detY / detH;
    const double ds = detS / detH;

    // 偏移过大说明极值应落在相邻采样点，简化处理直接剔除（不做迭代）。
    if (std::abs(dx) > 0.5 || std::abs(dy) > 0.5 || std::abs(ds) > 0.5) {
        return false;
    }

    // 插值点处的对比度：|D(x̂)| = D + 0.5 * g · offset。
    const double dHat = v + 0.5 * (gx * dx + gy * dy + gs * ds);
    if (std::abs(dHat) < contrastThreshold) {
        return false;
    }

    refinedY = y + dy;
    refinedX = x + dx;
    layerOffset = ds;
    refinedResponse = std::abs(dHat);
    return true;
}

// 用 CV_32F 灰度图按中心差分计算梯度幅值/方向。
void ComputeGradientOrientation(const cv::Mat& grayF, int y, int x, double& magnitude, double& orientation)
{
    const int rows = grayF.rows;
    const int cols = grayF.cols;
    const int yp = std::min(y + 1, rows - 1);
    const int ym = std::max(y - 1, 0);
    const int xp = std::min(x + 1, cols - 1);
    const int xm = std::max(x - 1, 0);
    const double dx = static_cast<double>(grayF.at<float>(y, xp)) - grayF.at<float>(y, xm);
    const double dy = static_cast<double>(grayF.at<float>(yp, x)) - grayF.at<float>(ym, x);
    magnitude = std::sqrt(dx * dx + dy * dy);
    orientation = std::atan2(dy, dx);
}

// ---------------------------------------------------------------------------
// 阶段 3：方向分配（36 bin 加权直方图，高斯窗加权）
//
// 在 octave-local 坐标系下计算，输入为关键点所在 octave 的高斯层，sigma 为
// octave-local 尺度（sigma * k^layer）。
// ---------------------------------------------------------------------------

std::vector<double> ComputeOrientations(const cv::Mat& grayF, double x, double y, double sigma)
{
    const double radius = 3.0 * 1.5 * sigma;
    const int r = static_cast<int>(std::ceil(radius));
    std::vector<double> hist(kOrientationBins, 0.0);

    for (int dy = -r; dy <= r; ++dy) {
        const int py = static_cast<int>(std::round(y)) + dy;
        if (py < 0 || py >= grayF.rows) {
            continue;
        }
        for (int dx = -r; dx <= r; ++dx) {
            const int px = static_cast<int>(std::round(x)) + dx;
            if (px < 0 || px >= grayF.cols) {
                continue;
            }
            const double dist2 = static_cast<double>(dx * dx + dy * dy);
            if (dist2 > radius * radius) {
                continue;
            }
            double mag = 0.0;
            double ori = 0.0;
            ComputeGradientOrientation(grayF, py, px, mag, ori);
            // 高斯加权：以关键点为中心，sigma = 1.5 * scale。
            const double w = std::exp(-dist2 / (2.0 * (1.5 * sigma) * (1.5 * sigma)));
            const int bin =
                static_cast<int>(std::floor(NormalizeAngle(ori) / (2.0 * kPi) * kOrientationBins)) % kOrientationBins;
            hist[bin] += w * mag;
        }
    }

    // 平滑直方图（循环）。
    std::vector<double> smooth(kOrientationBins, 0.0);
    for (int i = 0; i < kOrientationBins; ++i) {
        smooth[i] = 0.25 * hist[(i - 1 + kOrientationBins) % kOrientationBins] + 0.5 * hist[i] +
                    0.25 * hist[(i + 1) % kOrientationBins];
    }

    const double maxVal = *std::max_element(smooth.begin(), smooth.end());
    std::vector<double> orientations;
    if (maxVal <= 0.0) {
        return orientations;
    }
    const double peakThreshold = kOrientationPeakRatio * maxVal;
    for (int i = 0; i < kOrientationBins; ++i) {
        const double prev = smooth[(i - 1 + kOrientationBins) % kOrientationBins];
        const double next = smooth[(i + 1) % kOrientationBins];
        if (smooth[i] >= peakThreshold && smooth[i] >= prev && smooth[i] >= next) {
            // 抛物线插值求亚 bin 峰值。
            const double denom = 2.0 * (prev + next - 2.0 * smooth[i]);
            double offset = 0.0;
            if (std::abs(denom) > 1e-9) {
                offset = (prev - next) / denom;
            }
            const double binCenter = (static_cast<double>(i) + offset + 0.5) / kOrientationBins * (2.0 * kPi);
            orientations.push_back(NormalizeAngle(binCenter));
        }
    }
    return orientations;
}

// ---------------------------------------------------------------------------
// 阶段 4：128 维描述子生成（octave-local 坐标系）
// ---------------------------------------------------------------------------

void ComputeDescriptor(const cv::Mat& grayF, double x, double y, double sigma, double orientation,
                       std::vector<float>& descriptor)
{
    descriptor.assign(kDescriptorSize, 0.0f);

    // 描述子窗口：4x4 子块，每子块宽 4*sigma，覆盖半径约 8*sigma。
    const double windowRadius = 8.0 * sigma;
    const double cosT = std::cos(orientation);
    const double sinT = std::sin(orientation);

    const int kx = static_cast<int>(std::round(x));
    const int ky = static_cast<int>(std::round(y));

    // 采样步长 = sigma，采样范围为 windowRadius。
    const int samples = static_cast<int>(std::ceil(windowRadius / sigma));
    for (int sy = -samples; sy <= samples; ++sy) {
        for (int sx = -samples; sx <= samples; ++sx) {
            const double ox = sx * sigma;
            const double oy = sy * sigma;

            // 旋转到主方向坐标系（R(-θ)：把采样点偏移转到主方向对齐的坐标系）。
            const double rx = cosT * ox + sinT * oy;
            const double ry = -sinT * ox + cosT * oy;

            // 映射到 4x4 子块索引（每个子块宽 4*sigma）。
            const double bx = rx / (4.0 * sigma) + 2.0;
            const double by = ry / (4.0 * sigma) + 2.0;
            if (bx < -0.5 || bx >= 3.5 || by < -0.5 || by >= 3.5) {
                continue;
            }
            const int binX = std::min(3, std::max(0, static_cast<int>(std::floor(bx))));
            const int binY = std::min(3, std::max(0, static_cast<int>(std::floor(by))));

            const int px = kx + static_cast<int>(std::round(ox));
            const int py = ky + static_cast<int>(std::round(oy));
            if (px <= 0 || px >= grayF.cols - 1 || py <= 0 || py >= grayF.rows - 1) {
                continue;
            }

            double mag = 0.0;
            double ori = 0.0;
            ComputeGradientOrientation(grayF, py, px, mag, ori);
            // 相对主方向。
            const double relOri = NormalizeAngle(ori - orientation);

            // 高斯加权（按旋转后的半径，sigma = windowRadius/2）。
            const double gaussSigma = windowRadius / 2.0;
            const double w = std::exp(-(rx * rx + ry * ry) / (2.0 * gaussSigma * gaussSigma));

            const int obin =
                static_cast<int>(std::floor(relOri / (2.0 * kPi) * kDescriptorHistBins)) % kDescriptorHistBins;

            // 写入（OpenCV 描述子排列：每子块连续 8 个方向 bin）。
            const int base = (binY * kDescriptorHistWidth + binX) * kDescriptorHistBins;
            descriptor[base + obin] += static_cast<float>(w * mag);
        }
    }

    // L2 归一化 -> 截断 0.2 -> 再 L2 归一化（光照不变 + 抗非线性光照）。
    double norm = 0.0;
    for (float v : descriptor) {
        norm += static_cast<double>(v) * v;
    }
    norm = std::sqrt(norm);
    if (norm < 1e-12) {
        return;
    }
    for (float& v : descriptor) {
        v = static_cast<float>(v / norm);
        if (v > kDescriptorClip) {
            v = kDescriptorClip;
        }
    }
    norm = 0.0;
    for (float v : descriptor) {
        norm += static_cast<double>(v) * v;
    }
    norm = std::sqrt(norm);
    if (norm < 1e-12) {
        return;
    }
    for (float& v : descriptor) {
        v = static_cast<float>(v / norm);
    }
}

} // namespace

void SiftDetector::DetectAndCompute(const cv::Mat& gray, std::vector<cv::KeyPoint>& keypoints,
                                    cv::Mat& descriptors) const
{
    keypoints.clear();
    descriptors = cv::Mat();
    if (gray.empty() || gray.type() != CV_8UC1) {
        return;
    }

    const ScaleSpace ss = BuildScaleSpace(gray, params_.nOctaveLayers, params_.sigma);

    // 收集候选关键点：每个 octave 的中间层做 3x3x3 极值检测 + 对比度/边缘剔除。
    // 关键点记录为 (octave, layer, y, x)，坐标与 sigma 均为 octave-local。
    struct Candidate {
        int octave;
        int layer; // 整数尺度层（亚像素尺度偏移 layerOffset 单独记）
        int gridY; // 检测时的整数网格坐标（用于 NMS 邻域标记）
        int gridX;
        double y; // 亚像素精化后的坐标（octave-local）
        double x;
        double sigmaLocal; // octave-local 尺度 = sigma * k^(layer + layerOffset)
        double response;   // |DoG| 响应值，用于空间非极大值抑制排序
    };
    std::vector<Candidate> candidates;

    const double k = std::pow(2.0, 1.0 / params_.nOctaveLayers);
    for (int o = 0; o < static_cast<int>(ss.dog.size()); ++o) {
        for (int l = 1; l < static_cast<int>(ss.dog[o].size()) - 1; ++l) {
            const cv::Mat& layer = ss.dog[o][l];
            for (int gy = 1; gy < layer.rows - 1; ++gy) {
                for (int gx = 1; gx < layer.cols - 1; ++gx) {
                    if (!IsExtremum(ss.dog, o, l, gy, gx)) {
                        continue;
                    }

                    // 亚像素定位（Taylor 展开）+ 精化对比度剔除。
                    double refinedY = 0.0;
                    double refinedX = 0.0;
                    double layerOffset = 0.0;
                    double refinedResponse = 0.0;
                    if (!RefineExtremum(ss.dog, o, l, gy, gx, params_.contrastThreshold, refinedY, refinedX,
                                        layerOffset, refinedResponse)) {
                        continue;
                    }

                    // 边缘响应剔除：2x2 Hessian 迹/行列式比（曲率比）。
                    const double dxx = static_cast<double>(layer.at<float>(gy, gx + 1)) + layer.at<float>(gy, gx - 1) -
                                       2.0 * layer.at<float>(gy, gx);
                    const double dyy = static_cast<double>(layer.at<float>(gy + 1, gx)) + layer.at<float>(gy - 1, gx) -
                                       2.0 * layer.at<float>(gy, gx);
                    const double dxy =
                        (static_cast<double>(layer.at<float>(gy + 1, gx + 1)) - layer.at<float>(gy + 1, gx - 1) -
                         layer.at<float>(gy - 1, gx + 1) + layer.at<float>(gy - 1, gx - 1)) /
                        4.0;
                    const double trace = dxx + dyy;
                    const double det = dxx * dyy - dxy * dxy;
                    const double r = params_.edgeThreshold;
                    const double ratio = (r + 1.0) * (r + 1.0) / r;
                    if (det <= 0.0 || trace * trace / det >= ratio) {
                        continue;
                    }

                    const double sigmaLocal = params_.sigma * std::pow(k, l + layerOffset);
                    candidates.push_back(Candidate{o, l, gy, gx, refinedY, refinedX, sigmaLocal, refinedResponse});
                }
            }
        }
    }

    // 空间非极大值抑制：同一 (octave, layer) 内按响应从强到弱贪心保留，
    // 抑制落在已保留关键点 nmsWindowSize 邻域内的弱响应候选（解决大图关键点过多问题）。
    if (params_.nmsWindowSize > 1) {
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            if (a.octave != b.octave) {
                return a.octave < b.octave;
            }
            if (a.layer != b.layer) {
                return a.layer < b.layer;
            }
            return a.response > b.response;
        });

        const int half = params_.nmsWindowSize / 2;
        std::vector<Candidate> kept;
        kept.reserve(candidates.size());

        int curOctave = -1;
        int curLayer = -1;
        std::vector<std::vector<bool>> suppressed;
        int layerRows = 0;
        int layerCols = 0;

        for (const Candidate& c : candidates) {
            if (c.octave != curOctave || c.layer != curLayer) {
                curOctave = c.octave;
                curLayer = c.layer;
                layerRows = ss.gaussian[c.octave][c.layer].rows;
                layerCols = ss.gaussian[c.octave][c.layer].cols;
                suppressed.assign(layerRows, std::vector<bool>(layerCols, false));
            }
            if (suppressed[c.gridY][c.gridX]) {
                continue;
            }
            kept.push_back(c);
            for (int dy = -half; dy <= half; ++dy) {
                for (int dx = -half; dx <= half; ++dx) {
                    const int ny = c.gridY + dy;
                    const int nx = c.gridX + dx;
                    if (ny >= 0 && ny < layerRows && nx >= 0 && nx < layerCols) {
                        suppressed[ny][nx] = true;
                    }
                }
            }
        }
        candidates = std::move(kept);
    }

    // 阶段 3+4：方向 + 描述子，全部在 octave-local 坐标系下计算。
    std::vector<std::vector<float>> descRows;
    for (const Candidate& c : candidates) {
        const cv::Mat& octaveImage = ss.gaussian[c.octave][c.layer];

        const auto orientations = ComputeOrientations(octaveImage, c.x, c.y, c.sigmaLocal);
        for (double ori : orientations) {
            std::vector<float> desc;
            ComputeDescriptor(octaveImage, c.x, c.y, c.sigmaLocal, ori, desc);
            if (desc.empty()) {
                continue;
            }

            // 坐标/尺度映射回原图（乘 2^octave）。
            const double scale2 = std::pow(2.0, c.octave);
            keypoints.emplace_back(cv::Point2f(static_cast<float>(c.x * scale2), static_cast<float>(c.y * scale2)),
                                   static_cast<float>(c.sigmaLocal * scale2), static_cast<float>(ori * 180.0 / kPi),
                                   static_cast<float>(c.sigmaLocal * scale2));
            descRows.push_back(std::move(desc));
        }
    }

    if (keypoints.empty()) {
        return;
    }

    descriptors = cv::Mat(static_cast<int>(descRows.size()), kDescriptorSize, CV_32F);
    for (int i = 0; i < static_cast<int>(descRows.size()); ++i) {
        std::copy(descRows[i].begin(), descRows[i].end(), descriptors.ptr<float>(i));
    }
}

} // namespace FeatureMatching

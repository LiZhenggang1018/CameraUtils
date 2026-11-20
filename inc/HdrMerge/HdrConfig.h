/*
 * @Description: HDR 融合配置（Mertens / 物理 HDR 双管线 + 鬼影抑制）
 */
#ifndef INC_HDRMERGE_HDRCONFIG_H
#define INC_HDRMERGE_HDRCONFIG_H

#include "CommonEnv.h"
#include "JsonUtils.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

// ============================================================
// 质量档位参数集合
// ============================================================
struct QualityPreset {
    double contrastWeight = 1.2;
    double saturationWeight = 1.1;
    double exposureWeight = 1.0;
    bool enableUnsharpMask = true;
    double unsharpAmount = 0.6;
    double unsharpRadius = 1.2;
    double unsharpThreshold = 0.0;
    bool enableDetailEnhance = false;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(QualityPreset, contrastWeight, saturationWeight, exposureWeight,
                                                enableUnsharpMask, unsharpAmount, unsharpRadius, unsharpThreshold,
                                                enableDetailEnhance)
};

// ============================================================
// HDR 融合配置
// ============================================================
struct HdrConfig {
    // ---- 管线模式 ----
    std::string pipelineMode = "mertens"; // "mertens" | "physical"

    // ---- 质量档位 ----
    int qualityLevel = 1;

    std::map<std::string, QualityPreset> qualityPresets = {
        {"0", QualityPreset{.contrastWeight = 0.8,
                            .saturationWeight = 1.0,
                            .exposureWeight = 1.0,
                            .enableUnsharpMask = false,
                            .unsharpAmount = 0.6,
                            .unsharpRadius = 1.2,
                            .unsharpThreshold = 0.0,
                            .enableDetailEnhance = false}},
        {"1", QualityPreset{.contrastWeight = 1.2,
                            .saturationWeight = 1.1,
                            .exposureWeight = 1.0,
                            .enableUnsharpMask = true,
                            .unsharpAmount = 0.6,
                            .unsharpRadius = 1.2,
                            .unsharpThreshold = 0.0,
                            .enableDetailEnhance = false}},
        {"2", QualityPreset{.contrastWeight = 1.5,
                            .saturationWeight = 1.2,
                            .exposureWeight = 1.2,
                            .enableUnsharpMask = true,
                            .unsharpAmount = 1.0,
                            .unsharpRadius = 1.5,
                            .unsharpThreshold = 2.0,
                            .enableDetailEnhance = false}},
    };

    // ---- 细粒度字段 ----
    double contrastWeight = 1.2;
    double saturationWeight = 1.1;
    double exposureWeight = 1.0;

    bool autoDetectBorder = true;
    double cropRatio = 0.02;
    int blackThreshold = 5;
    double blackRatio = 0.95;

    bool enableUnsharpMask = true;
    double unsharpAmount = 0.6;
    double unsharpRadius = 1.2;
    double unsharpThreshold = 0.0;

    bool enableDetailEnhance = false;
    double detailSigmaS = 10.0;
    double detailSigmaR = 0.15;

    // ---- 物理 HDR ----
    bool readExposureFromExif = true;
    std::vector<double> manualExposureTimes;
    std::vector<double> manualIsos;
    double baseIso = 100.0;

    bool calibrateResponse = false;
    std::string tonemapType = "drago";

    double dragoGamma = 1.0;
    double dragoSaturation = 1.0;
    double reinhardGamma = 1.0;
    double reinhardIntensity = 0.0;
    double reinhardLightAdapt = 0.0;

    // ---- 鬼影抑制 ----
    //   ghostDetectionMode:
    //     "diff"      : 亮度归一化帧差（默认，主力）
    //     "flow"      : 双向 LK 反推不稳定点（补充）
    //     "flow+diff" : 两者并集
    bool enableGhostSuppression = false;
    std::string ghostDetectionMode = "diff";
    double motionThreshold = 15.0;      // Level 1 帧差阈值（0~255 灰度）
    double lkFbThreshold = 1.5;         // Level 2 LK 前后向误差阈值（px）
    int motionDilateRadius = 20;        // mask 稠密化/膨胀半径（px）
    double ghostSuppressStrength = 1.0; // 0~1，mask 回填强度
    bool saveMotionMask = false;        // 输出 06_motion_mask.png

    bool saveIntermediate = true;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        HdrConfig, pipelineMode, qualityLevel, qualityPresets, contrastWeight, saturationWeight, exposureWeight,
        autoDetectBorder, cropRatio, blackThreshold, blackRatio, enableUnsharpMask, unsharpAmount, unsharpRadius,
        unsharpThreshold, enableDetailEnhance, detailSigmaS, detailSigmaR, readExposureFromExif, manualExposureTimes,
        manualIsos, baseIso, calibrateResponse, tonemapType, dragoGamma, dragoSaturation, reinhardGamma,
        reinhardIntensity, reinhardLightAdapt, enableGhostSuppression, ghostDetectionMode, motionThreshold,
        lkFbThreshold, motionDilateRadius, ghostSuppressStrength, saveMotionMask, saveIntermediate)

    static const std::string& DefaultPath()
    {
        static const std::string p = EnvRootPath() + "/configs/HdrConfig.json";
        return p;
    }

    void ApplyQualityLevel()
    {
        if (qualityLevel < 0) {
            return;
        }

        const std::string key = std::to_string(qualityLevel);
        auto it = qualityPresets.find(key);
        if (it == qualityPresets.end()) {
            std::fprintf(stderr,
                         "[HdrConfig] Warning: qualityLevel=%d but qualityPresets has no key \"%s\", "
                         "falling back to fine-grained fields\n",
                         qualityLevel, key.c_str());
            return;
        }

        const QualityPreset& p = it->second;
        contrastWeight = p.contrastWeight;
        saturationWeight = p.saturationWeight;
        exposureWeight = p.exposureWeight;
        enableUnsharpMask = p.enableUnsharpMask;
        unsharpAmount = p.unsharpAmount;
        unsharpRadius = p.unsharpRadius;
        unsharpThreshold = p.unsharpThreshold;
        enableDetailEnhance = p.enableDetailEnhance;
    }

    static HdrConfig LoadFromFile(const std::string& path)
    {
        auto opt = Dsrlz<HdrConfig>(path);
        if (opt.has_value()) {
            HdrConfig cfg = opt.value();
            cfg.ApplyQualityLevel();
            std::printf("[HdrConfig] Loaded: %s (pipeline=%s, qualityLevel=%d, presets=%zu, "
                        "tonemap=%s, ghost=%d/%s, effective contrast=%.2f)\n",
                        path.c_str(), cfg.pipelineMode.c_str(), cfg.qualityLevel, cfg.qualityPresets.size(),
                        cfg.tonemapType.c_str(), static_cast<int>(cfg.enableGhostSuppression),
                        cfg.ghostDetectionMode.c_str(), cfg.contrastWeight);
            return cfg;
        }

        std::printf("[HdrConfig] %s not found or invalid, creating default\n", path.c_str());
        HdrConfig def;
        Srlz(def, path);
        return def;
    }

    void SaveToFile(const std::string& path) const { Srlz(*this, path); }
};


#endif // INC_HDRMERGE_HDRCONFIG_H
/*
 * @Description: HDR 融合配置（支持 Mertens / 物理 HDR 双管线）
 */
#ifndef HDR_CONFIG_H
#define HDR_CONFIG_H

#include "JsonUtils.h"
#include "CommonEnv.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

// ============================================================
// 质量档位参数集合
// ============================================================
struct QualityPreset
{
    double contrastWeight      = 1.2;
    double saturationWeight    = 1.1;
    double exposureWeight      = 1.0;
    bool   enableUnsharpMask   = true;
    double unsharpAmount       = 0.6;
    double unsharpRadius       = 1.2;
    double unsharpThreshold    = 0.0;
    bool   enableDetailEnhance = false;

    // _WITH_DEFAULT：JSON 缺失字段用类内默认值，兼容旧配置文件
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(QualityPreset,
        contrastWeight, saturationWeight, exposureWeight,
        enableUnsharpMask, unsharpAmount, unsharpRadius, unsharpThreshold,
        enableDetailEnhance)
};

// ============================================================
// HDR 融合配置
// ============================================================
struct HdrConfig
{
    // ---- 管线模式 ----
    //   "mertens"  : 现有 Mertens 曝光融合（默认，向后兼容）
    //   "physical" : EXIF 曝光 + De-gamma + MergeDebevec + Tonemap
    std::string pipelineMode = "mertens";

    // ---- 质量档位 ----
    int qualityLevel = 1;

    std::map<std::string, QualityPreset> qualityPresets = {
        { "0", QualityPreset{ 0.8, 1.0, 1.0, false, 0.6, 1.2, 0.0, false } },
        { "1", QualityPreset{ 1.2, 1.1, 1.0, true,  0.6, 1.2, 0.0, false } },
        { "2", QualityPreset{ 1.5, 1.2, 1.2, true,  1.0, 1.5, 2.0, false } },
    };

    // ---- 细粒度字段 ----
    double contrastWeight   = 1.2;
    double saturationWeight = 1.1;
    double exposureWeight   = 1.0;

    bool   autoDetectBorder = true;
    double cropRatio        = 0.02;
    int    blackThreshold   = 5;
    double blackRatio       = 0.95;

    bool   enableUnsharpMask = true;
    double unsharpAmount     = 0.6;
    double unsharpRadius     = 1.2;
    double unsharpThreshold  = 0.0;

    bool   enableDetailEnhance = false;
    double detailSigma_s       = 10.0;
    double detailSigma_r       = 0.15;

    // ---- 物理 HDR ----
    bool   readExposureFromExif = true;
    std::vector<double> manualExposureTimes;  // 秒，size=3（EXIF 失败时回退）
    std::vector<double> manualIsos;           // size=3
    double baseIso = 100.0;                   // 等效曝光量归一化基准

    bool   calibrateResponse = false;         // 是否用 CalibrateDebevec 估计 CRF
    std::string tonemapType  = "drago";       // "drago" | "reinhard"

    double dragoGamma        = 1.0;
    double dragoSaturation   = 1.0;
    double reinhardGamma     = 1.0;
    double reinhardIntensity = 0.0;           // 0 = 自动
    double reinhardLightAdapt = 0.0;          // 0 = 自动

    bool   saveIntermediate = true;

    // _WITH_DEFAULT：新增字段在旧 JSON 缺失时使用类内默认值，不会导致加载失败
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(HdrConfig,
        pipelineMode,
        qualityLevel,
        qualityPresets,
        contrastWeight, saturationWeight, exposureWeight,
        autoDetectBorder, cropRatio, blackThreshold, blackRatio,
        enableUnsharpMask, unsharpAmount, unsharpRadius, unsharpThreshold,
        enableDetailEnhance, detailSigma_s, detailSigma_r,
        readExposureFromExif, manualExposureTimes, manualIsos, baseIso,
        calibrateResponse, tonemapType,
        dragoGamma, dragoSaturation,
        reinhardGamma, reinhardIntensity, reinhardLightAdapt,
        saveIntermediate)

    static const std::string& DefaultPath()
    {
        static const std::string p = EnvRootPath() + "/configs/HdrConfig.json";
        return p;
    }

    // 用 preset 覆盖受档位影响的 8 个字段
    void ApplyQualityLevel()
    {
        if (qualityLevel < 0)
        {
            return;
        }

        const std::string key = std::to_string(qualityLevel);
        auto it = qualityPresets.find(key);
        if (it == qualityPresets.end())
        {
            std::fprintf(stderr,
                "[HdrConfig] Warning: qualityLevel=%d but qualityPresets has no key \"%s\", "
                "falling back to fine-grained fields\n",
                qualityLevel, key.c_str());
            return;
        }

        const QualityPreset& p = it->second;
        contrastWeight      = p.contrastWeight;
        saturationWeight    = p.saturationWeight;
        exposureWeight      = p.exposureWeight;
        enableUnsharpMask   = p.enableUnsharpMask;
        unsharpAmount       = p.unsharpAmount;
        unsharpRadius       = p.unsharpRadius;
        unsharpThreshold    = p.unsharpThreshold;
        enableDetailEnhance = p.enableDetailEnhance;
    }

    static HdrConfig LoadFromFile(const std::string& path)
    {
        auto opt = Dsrlz<HdrConfig>(path);
        if (opt.has_value())
        {
            HdrConfig cfg = opt.value();
            cfg.ApplyQualityLevel();
            std::printf("[HdrConfig] Loaded: %s (pipeline=%s, qualityLevel=%d, presets=%zu, "
                        "tonemap=%s, effective contrast=%.2f, unsharpAmount=%.2f)\n",
                        path.c_str(), cfg.pipelineMode.c_str(), cfg.qualityLevel,
                        cfg.qualityPresets.size(), cfg.tonemapType.c_str(),
                        cfg.contrastWeight, cfg.unsharpAmount);
            return cfg;
        }

        std::printf("[HdrConfig] %s not found or invalid, creating default\n", path.c_str());
        HdrConfig def;
        Srlz(def, path);
        return def;
    }

    void SaveToFile(const std::string& path) const
    {
        Srlz(*this, path);
    }
};

#endif // HDR_CONFIG_H
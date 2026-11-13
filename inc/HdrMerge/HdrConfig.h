/*
 * @Description: HDR 融合配置（支持 JSON 序列化 + 可配置的质量档位）
 *
 * 设计：
 *   - QualityPreset：单个档位的参数集合
 *   - HdrConfig::qualityPresets：档位定义表（key = "0"/"1"/"2"，value = QualityPreset）
 *   - qualityLevel：选哪一档；-1 = 自定义（用细粒度字段）
 *   - 所有档位数值都可以在 JSON 里调整，无需重编译
 */

#ifndef HDR_CONFIG_H
#define HDR_CONFIG_H

#include "JsonUtils.h"
#include "CommonEnv.h"

#include <cstdio>
#include <map>
#include <string>

// ============================================================
// 质量档位的参数集合
//   - 一个 QualityPreset 就是"一套可调参数"
//   - 通过 HdrConfig::qualityPresets 可以增删/修改档位
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

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(QualityPreset,
        contrastWeight, saturationWeight, exposureWeight,
        enableUnsharpMask, unsharpAmount, unsharpRadius, unsharpThreshold,
        enableDetailEnhance)
};

// ============================================================
// HDR 融合配置
//
// 配置文件：<项目根>/configs/HdrConfig.json
// 首次运行自动生成默认配置；之后改 JSON 无需重编译
// ============================================================
struct HdrConfig
{
    // ---- 质量档位选择 ----
    //   >= 0 → 从 qualityPresets 里找 key = std::to_string(qualityLevel) 的 preset
    //   -1  → 不覆盖，使用下面的细粒度字段
    int qualityLevel = 1;

    // ---- 档位定义表（可在 JSON 里增删改）----
    // 默认提供 3 档；用户可以在 JSON 里加 "3" / "4" 等新档位
    std::map<std::string, QualityPreset> qualityPresets = {
        { "0", QualityPreset{ 0.8, 1.0, 1.0, false, 0.6, 1.2, 0.0, false } },  // 柔和
        { "1", QualityPreset{ 1.2, 1.1, 1.0, true,  0.6, 1.2, 0.0, false } },  // 平衡
        { "2", QualityPreset{ 1.5, 1.2, 1.2, true,  1.0, 1.5, 2.0, false } },  // 锐利
    };

    // ---- 细粒度字段（qualityLevel = -1 时生效）----
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

    bool   saveIntermediate = true;

    // 一行宏完成所有字段的序列化/反序列化
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(HdrConfig,
        qualityLevel,
        qualityPresets,
        contrastWeight, saturationWeight, exposureWeight,
        autoDetectBorder, cropRatio, blackThreshold, blackRatio,
        enableUnsharpMask, unsharpAmount, unsharpRadius, unsharpThreshold,
        enableDetailEnhance, detailSigma_s, detailSigma_r,
        saveIntermediate)

    static const std::string& DefaultPath()
    {
        static const std::string p = EnvRootPath() + "/configs/HdrConfig.json";
        return p;
    }

    // 从 qualityPresets 里取对应档位，覆盖细粒度字段
    void ApplyQualityLevel()
    {
        if (qualityLevel < 0)
        {
            // -1 或更小：不覆盖，使用细粒度字段
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
            std::printf("[HdrConfig] Loaded: %s (qualityLevel=%d, presets=%zu, "
                        "effective contrast=%.2f, unsharpAmount=%.2f, enableDetailEnhance=%d)\n",
                        path.c_str(), cfg.qualityLevel, cfg.qualityPresets.size(),
                        cfg.contrastWeight, cfg.unsharpAmount,
                        static_cast<int>(cfg.enableDetailEnhance));
            return cfg;
        }

        std::printf("[HdrConfig] %s not found, creating default\n", path.c_str());
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
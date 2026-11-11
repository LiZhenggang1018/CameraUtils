#ifndef INTERPOLATION_CONFIG_H
#define INTERPOLATION_CONFIG_H

#include "JsonUtils.h"
#include "CommonEnv.h"

#include <cstdio>
#include <string>

struct InterpolationConfig {
    // ---- 插帧 ----
    int         factor              = 2;

    // ---- 编码 ----
    int         bitrate             = 16000000;
    std::string preset              = "veryfast";
    int         crf                 = 20;

    // ---- 性能档位（新增） ----
    // 0=极速, 1=快, 2=平衡, 3=画质, 4=极致(TV-L1)
    // 设为 -1 表示"不启用档位预设",直接使用下面细粒度字段的值
    int         performanceLevel    = -1;

    // ---- 光流后端 ----
    int         flowBackend         = 1;
    int         disPreset           = 0;
    int         maxCoarseLevels     = 1;

    // ---- 性能细粒度开关（新增） ----
    double      flowDownscale           = 1.0;
    bool        enableBilateralPreprocess = true;
    bool        enableFlowPostProcess     = true;
    bool        enableLaplacianBlend      = true;
    int         laplacianLevels           = 4;

    // ---- 质量参数 ----
    double      fbThreshold         = 1.0;
    bool        enableStaticProtect = true;
    double      staticMagThreshold  = 1.5;
    double      confidenceBlurSigma = 3.0;
    double      occlusionBlurSigma  = 2.0;
    bool        enableTemporalSmooth = true;
    double      temporalAlpha        = 0.85;
    double      temporalMagThreshold = 5.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(InterpolationConfig,
        factor,
        bitrate, preset, crf,
        performanceLevel,
        flowBackend, disPreset, maxCoarseLevels,
        flowDownscale, enableBilateralPreprocess, enableFlowPostProcess,
        enableLaplacianBlend, laplacianLevels,
        fbThreshold,
        enableStaticProtect, staticMagThreshold,
        confidenceBlurSigma, occlusionBlurSigma,
        enableTemporalSmooth, temporalAlpha, temporalMagThreshold)

    static const std::string& DefaultPath() {
        static const std::string p = EnvRootPath() + "/configs/InterpolationConfig.json";
        return p;
    }

    // 根据 performanceLevel 覆盖细粒度字段
    void ApplyPerformanceLevel() {
        switch (performanceLevel) {
            case 0:   // 极速（~10 秒）
                flowBackend = 1;
                disPreset = 0;
                maxCoarseLevels = 0;
                flowDownscale = 0.25;
                enableBilateralPreprocess = false;
                enableFlowPostProcess = false;
                enableLaplacianBlend = false;
                enableTemporalSmooth = false;
                break;

            case 1:   // 快（~20 秒）
                flowBackend = 1;
                disPreset = 0;
                maxCoarseLevels = 0;
                flowDownscale = 0.5;
                enableBilateralPreprocess = false;
                enableFlowPostProcess = false;
                enableLaplacianBlend = true;
                laplacianLevels = 3;
                enableTemporalSmooth = true;
                break;

            case 2:   // 平衡（~45 秒，默认）
                flowBackend = 1;
                disPreset = 1;
                maxCoarseLevels = 1;
                flowDownscale = 1.0;
                enableBilateralPreprocess = true;
                enableFlowPostProcess = true;
                enableLaplacianBlend = true;
                laplacianLevels = 4;
                enableTemporalSmooth = true;
                break;

            case 3:   // 画质（~70 秒）
                flowBackend = 1;
                disPreset = 2;
                maxCoarseLevels = 2;
                flowDownscale = 1.0;
                enableBilateralPreprocess = true;
                enableFlowPostProcess = true;
                enableLaplacianBlend = true;
                laplacianLevels = 4;
                enableTemporalSmooth = true;
                break;

            case 4:   // 极致（~3~5 分钟，TV-L1）
                flowBackend = 2;
                flowDownscale = 0.5;   // TV-L1 1/2 分辨率，避免 5 分钟以上
                enableBilateralPreprocess = true;
                enableFlowPostProcess = true;
                enableLaplacianBlend = true;
                laplacianLevels = 4;
                enableTemporalSmooth = true;
                break;

            default:  // -1 或其他：不覆盖，使用细粒度字段
                break;
        }
    }

    static InterpolationConfig LoadFromFile(const std::string& path) {
        auto opt = Dsrlz<InterpolationConfig>(path);
        if (opt.has_value()) {
            InterpolationConfig cfg = opt.value();
            cfg.ApplyPerformanceLevel();
            std::printf("[InterpolationConfig] Loaded: %s "
                        "(factor=%d, performanceLevel=%d, flowBackend=%d, flowDownscale=%.2f)\n",
                        path.c_str(), cfg.factor, cfg.performanceLevel,
                        cfg.flowBackend, cfg.flowDownscale);
            return cfg;
        }
        std::printf("[InterpolationConfig] %s not found, creating default\n", path.c_str());
        InterpolationConfig def;
        Srlz(def, path);
        return def;
    }

    void SaveToFile(const std::string& path) const {
        Srlz(*this, path);
    }
};

#endif // INTERPOLATION_CONFIG_H
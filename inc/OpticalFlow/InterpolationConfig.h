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

    // ---- 光流后端 ----
    int         flowBackend         = 1;   // 0=Farneback, 1=DIS
    int         disPreset           = 0;   // 0=ULTRAFAST, 1=FAST, 2=MEDIUM
    int         maxCoarseLevels     = 1;   // 大位移处理：外部金字塔层数（0=禁用）

    // ---- 前后向一致性 ----
    double      fbThreshold         = 1.0;

    // ---- 静态区保护 ----
    bool        enableStaticProtect = true;
    double      staticMagThreshold  = 1.5;

    // ---- 遮挡区软化 ----
    double      confidenceBlurSigma = 3.0;
    double      occlusionBlurSigma  = 2.0;

    // ---- 时域平滑 ----
    bool        enableTemporalSmooth = true;
    double      temporalAlpha        = 0.85;
    double      temporalMagThreshold = 5.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(InterpolationConfig,
        factor,
        bitrate, preset, crf,
        flowBackend, disPreset, maxCoarseLevels,
        fbThreshold,
        enableStaticProtect, staticMagThreshold,
        confidenceBlurSigma, occlusionBlurSigma,
        enableTemporalSmooth, temporalAlpha, temporalMagThreshold)

    static const std::string& DefaultPath() {
        static const std::string p = EnvRootPath() + "/configs/InterpolationConfig.json";
        return p;
    }

    static InterpolationConfig LoadFromFile(const std::string& path) {
        auto opt = Dsrlz<InterpolationConfig>(path);
        if (opt.has_value()) {
            std::printf("[InterpolationConfig] Loaded: %s "
                        "(factor=%d, disPreset=%d, coarseLevels=%d, temporalAlpha=%.2f)\n",
                        path.c_str(),
                        opt->factor, opt->disPreset, opt->maxCoarseLevels,
                        opt->temporalAlpha);
            return opt.value();
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
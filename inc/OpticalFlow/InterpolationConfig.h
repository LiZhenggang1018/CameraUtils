#ifndef INTERPOLATION_CONFIG_H
#define INTERPOLATION_CONFIG_H

#include "JsonUtils.h"      // Srlz / Dsrlz
#include "CommonEnv.h"      // EnvRootPath()

#include <cstdio>
#include <string>

// 插帧配置
// 配置文件：<项目根>/configs/InterpolationConfig.json
// 首次运行自动生成默认配置，之后修改该文件即可调参，无需重新编译
struct InterpolationConfig {
    // 插帧倍率：2=60fps, 3=90fps, 4=120fps
    int         factor              = 2;

    // 编码参数（crf >= 0 时用 CRF 模式，忽略 bitrate）
    int         bitrate             = 16000000;
    std::string preset              = "veryfast";
    int         crf                 = 20;

    // 光流参数
    int         flowBackend         = 1;        // 1=DIS, 0=Farneback
    double      fbThreshold         = 1.0;

    // 静态区保护
    bool        enableStaticProtect = true;
    double      staticMagThreshold  = 1.5;

    // 权重软化
    double      confidenceBlurSigma = 3.0;
    double      occlusionBlurSigma  = 2.0;

    // 一行宏完成所有字段的序列化/反序列化
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(InterpolationConfig,
        factor,
        bitrate, preset, crf,
        flowBackend, fbThreshold,
        enableStaticProtect, staticMagThreshold,
        confidenceBlurSigma, occlusionBlurSigma)

    // 默认配置文件路径
    static const std::string& DefaultPath() {
        static const std::string p = EnvRootPath() + "/configs/InterpolationConfig.json";
        return p;
    }

    // 加载：文件不存在或解析失败 → 用默认值，同时生成一份模板
    static InterpolationConfig LoadFromFile(const std::string& path) {
        auto opt = Dsrlz<InterpolationConfig>(path);
        if (opt.has_value()) {
            std::printf("[InterpolationConfig] Loaded: %s (factor=%d, preset=%s, crf=%d)\n",
                        path.c_str(), opt->factor, opt->preset.c_str(), opt->crf);
            return opt.value();
        }
        // 首次运行：写一份默认配置，方便后续编辑
        std::printf("[InterpolationConfig] %s not found, creating default\n", path.c_str());
        InterpolationConfig def;
        Srlz(def, path);
        return def;
    }

    // 保存
    void SaveToFile(const std::string& path) const {
        Srlz(*this, path);
    }
};

#endif // INTERPOLATION_CONFIG_H
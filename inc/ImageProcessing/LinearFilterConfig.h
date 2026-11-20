#ifndef INC_IMAGEPROCESSING_LINEARFILTERCONFIG_H
#define INC_IMAGEPROCESSING_LINEARFILTERCONFIG_H

#include "CommonEnv.h"
#include "JsonUtils.h"

#include <cstdio>
#include <string>

// 线性滤波调参 demo 的配置（序列化/反序列化，参考 HdrConfig.h 模式）。
struct LinearFilterConfig {
    // filterType: "gaussian" | "mean" | "sharpen" | "sobel"
    std::string filterType = "gaussian";
    int kernelSize = 3;
    double sigma = 1.0;
    double sharpenAmount = 1.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(LinearFilterConfig, filterType, kernelSize, sigma, sharpenAmount)

    static const std::string& DefaultPath()
    {
        static const std::string p = EnvRootPath() + "/configs/LinearFilterConfig.json";
        return p;
    }

    static LinearFilterConfig LoadFromFile(const std::string& path)
    {
        auto opt = Dsrlz<LinearFilterConfig>(path);
        if (opt.has_value()) {
            LinearFilterConfig cfg = opt.value();
            std::printf(
                "[LinearFilterConfig] Loaded: %s (filterType=%s, kernelSize=%d, sigma=%.2f, sharpenAmount=%.2f)\n",
                path.c_str(), cfg.filterType.c_str(), cfg.kernelSize, cfg.sigma, cfg.sharpenAmount);
            return cfg;
        }
        std::printf("[LinearFilterConfig] %s not found or invalid, creating default\n", path.c_str());
        LinearFilterConfig def;
        Srlz(def, path);
        return def;
    }

    void SaveToFile(const std::string& path) const { Srlz(*this, path); }
};

#endif // INC_IMAGEPROCESSING_LINEARFILTERCONFIG_H

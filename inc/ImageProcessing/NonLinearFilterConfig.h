#ifndef INC_IMAGEPROCESSING_NONLINEARFILTERCONFIG_H
#define INC_IMAGEPROCESSING_NONLINEARFILTERCONFIG_H

#include "CommonEnv.h"
#include "JsonUtils.h"

#include <cstdio>
#include <string>

// 非线性滤波调参 demo 的配置（序列化/反序列化，参考 HdrConfig.h 模式）。
struct NonLinearFilterConfig {
    // filterType: "median" | "bilateral"
    std::string filterType = "median";
    int kernelSize = 3;
    int diameter = 9;
    double sigmaColor = 50.0;
    double sigmaSpace = 50.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(NonLinearFilterConfig, filterType, kernelSize, diameter, sigmaColor,
                                                sigmaSpace)

    static const std::string& DefaultPath()
    {
        static const std::string p = EnvRootPath() + "/configs/NonLinearFilterConfig.json";
        return p;
    }

    static NonLinearFilterConfig LoadFromFile(const std::string& path)
    {
        auto opt = Dsrlz<NonLinearFilterConfig>(path);
        if (opt.has_value()) {
            NonLinearFilterConfig cfg = opt.value();
            std::printf("[NonLinearFilterConfig] Loaded: %s (filterType=%s, kernelSize=%d, diameter=%d, "
                        "sigmaColor=%.2f, sigmaSpace=%.2f)\n",
                        path.c_str(), cfg.filterType.c_str(), cfg.kernelSize, cfg.diameter, cfg.sigmaColor,
                        cfg.sigmaSpace);
            return cfg;
        }
        std::printf("[NonLinearFilterConfig] %s not found or invalid, creating default\n", path.c_str());
        NonLinearFilterConfig def;
        Srlz(def, path);
        return def;
    }

    void SaveToFile(const std::string& path) const { Srlz(*this, path); }
};

#endif // INC_IMAGEPROCESSING_NONLINEARFILTERCONFIG_H

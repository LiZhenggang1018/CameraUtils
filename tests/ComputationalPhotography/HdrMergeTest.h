#ifndef TESTS_COMPUTATIONALPHOTOGRAPHY_HDRMERGETEST_H
#define TESTS_COMPUTATIONALPHOTOGRAPHY_HDRMERGETEST_H

#include "ComputationalPhotography/HdrMerge.h"

#include <iostream>
#include <string>
#include <vector>

// ============================================================
// 单次运行（Mertens，默认配置）
// ============================================================
inline void RunHdrMergeTest()
{
    const std::string dataDir = EnvRootPath() + "/data/HdrMerge/";
    const std::string lowPath = dataDir + "low.jpg";
    const std::string normalPath = dataDir + "normal.jpg";
    const std::string highPath = dataDir + "high.jpg";
    const std::string outDir = dataDir + "result/";

    std::cout << "========== HdrMerge begin ==========" << std::endl;
    std::cout << "[HdrMerge] lowPath    = " << lowPath << std::endl;
    std::cout << "[HdrMerge] normalPath = " << normalPath << std::endl;
    std::cout << "[HdrMerge] highPath   = " << highPath << std::endl;
    std::cout << "[HdrMerge] outDir     = " << outDir << std::endl;

    const HdrConfig cfg = HdrConfig::LoadFromFile(HdrConfig::DefaultPath());

    int rc = ComputationalPhotography::HdrMergeFromFiles(lowPath, normalPath, highPath, outDir, cfg);
    std::cout << "HdrMerge return code = " << rc << std::endl;
    std::cout << "========== HdrMerge end ============" << std::endl;
}

// ============================================================
// 多档位对比（Mertens）
// ============================================================
inline void RunHdrMergeMultiLevelTest()
{
    const std::string dataDir = EnvRootPath() + "/data/HdrMerge/";
    const std::string lowPath = dataDir + "low.jpg";
    const std::string normalPath = dataDir + "normal.jpg";
    const std::string highPath = dataDir + "high.jpg";

    std::cout << "========== HdrMerge Multi-Level begin ==========" << std::endl;

    const HdrConfig baseCfg = HdrConfig::LoadFromFile(HdrConfig::DefaultPath());

    if (baseCfg.qualityPresets.empty()) {
        std::cerr << "[HdrMergeMultiLevel] No qualityPresets defined, skip" << std::endl;
        return;
    }

    for (const auto& [key, preset] : baseCfg.qualityPresets) {
        int level = 0;
        try {
            level = std::stoi(key);
        } catch (...) {
            std::cerr << "[HdrMergeMultiLevel] Skip non-integer key: " << key << std::endl;
            continue;
        }

        std::cout << "\n--- [qualityLevel=" << level << " / key=\"" << key << "\"] ---" << std::endl;

        HdrConfig cfg = baseCfg;
        cfg.qualityLevel = level;
        cfg.ApplyQualityLevel();

        const std::string outDir = dataDir + "result_level_" + key + "/";
        int rc = ComputationalPhotography::HdrMergeFromFiles(lowPath, normalPath, highPath, outDir, cfg);
        std::cout << "[" << key << "] rc = " << rc << std::endl;
    }

    std::cout << "========== HdrMerge Multi-Level end ============" << std::endl;
}

// ============================================================
// 物理 HDR 测试（Drago / Reinhard）
// ============================================================
inline void RunHdrMergePhysicalTest()
{
    const std::string dataDir = EnvRootPath() + "/data/HdrMerge/";
    const std::string lowPath = dataDir + "low.jpg";
    const std::string normalPath = dataDir + "normal.jpg";
    const std::string highPath = dataDir + "high.jpg";

    std::cout << "========== HdrMerge Physical begin ==========" << std::endl;

    const HdrConfig baseCfg = HdrConfig::LoadFromFile(HdrConfig::DefaultPath());

    const std::vector<std::string> tonemapTypes = {"drago", "reinhard"};
    for (const auto& type : tonemapTypes) {
        HdrConfig cfg = baseCfg;
        cfg.pipelineMode = "physical";
        cfg.tonemapType = type;
        cfg.enableUnsharpMask = false;

        const std::string outDir = dataDir + "result_physical_" + type + "/";
        std::cout << "\n--- [pipeline=physical, tonemap=" << type << "] ---" << std::endl;

        int rc = ComputationalPhotography::HdrMergeFromFiles(lowPath, normalPath, highPath, outDir, cfg);
        std::cout << "[" << type << "] rc = " << rc << std::endl;
    }

    std::cout << "========== HdrMerge Physical end ============" << std::endl;
}

// ============================================================
// 鬼影抑制测试
//   1) Mertens + diff
//   2) Physical + diff（Drago）
//   3) Mertens + flow+diff
//   每次开启 saveMotionMask，便于人工核对检测结果
// ============================================================
inline void RunHdrMergeGhostSuppressionTest()
{
    const std::string dataDir = EnvRootPath() + "/data/HdrMerge/";
    const std::string lowPath = dataDir + "low.jpg";
    const std::string normalPath = dataDir + "normal.jpg";
    const std::string highPath = dataDir + "high.jpg";

    std::cout << "========== HdrMerge Ghost-Suppression begin ==========" << std::endl;

    const HdrConfig baseCfg = HdrConfig::LoadFromFile(HdrConfig::DefaultPath());

    struct Case {
        std::string label;
        std::string pipeline;
        std::string ghostMode;
    };

    const std::vector<Case> cases = {
        {.label = "ghost_mertens_diff", .pipeline = "mertens", .ghostMode = "diff"},
        {.label = "ghost_physical_diff", .pipeline = "physical", .ghostMode = "diff"},
        {.label = "ghost_mertens_flowdiff", .pipeline = "mertens", .ghostMode = "flow+diff"},
    };

    for (const auto& c : cases) {
        HdrConfig cfg = baseCfg;
        cfg.pipelineMode = c.pipeline;
        cfg.enableGhostSuppression = true;
        cfg.ghostDetectionMode = c.ghostMode;
        cfg.saveMotionMask = true;
        cfg.enableUnsharpMask = false; // 先看纯融合结果

        const std::string outDir = dataDir + "result_" + c.label + "/";
        std::cout << "\n--- [" << c.label << " | pipeline=" << c.pipeline << " | mode=" << c.ghostMode << "] ---"
                  << std::endl;

        int rc = ComputationalPhotography::HdrMergeFromFiles(lowPath, normalPath, highPath, outDir, cfg);
        std::cout << "[" << c.label << "] rc = " << rc << std::endl;
    }

    std::cout << "========== HdrMerge Ghost-Suppression end ============" << std::endl;
}

#endif // TESTS_COMPUTATIONALPHOTOGRAPHY_HDRMERGETEST_H
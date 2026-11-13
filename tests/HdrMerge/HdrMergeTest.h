#ifndef HDR_MERGE_TEST_H
#define HDR_MERGE_TEST_H

#include "HdrMerge/HdrMerge.h"

#include <iostream>

// ============================================================
// 单次运行：从 JSON 加载配置，跑一次 HDR 融合
// ============================================================
inline void RunHdrMergeTest()
{
    const std::string dataDir    = EnvRootPath() + "/data/HdrMerge/";
    const std::string lowPath    = dataDir + "low.jpg";
    const std::string normalPath = dataDir + "normal.jpg";
    const std::string highPath   = dataDir + "high.jpg";
    const std::string outDir     = dataDir + "result/";

    std::cout << "========== HdrMerge begin ==========" << std::endl;
    std::cout << "[HdrMerge] lowPath    = " << lowPath    << std::endl;
    std::cout << "[HdrMerge] normalPath = " << normalPath << std::endl;
    std::cout << "[HdrMerge] highPath   = " << highPath   << std::endl;
    std::cout << "[HdrMerge] outDir     = " << outDir     << std::endl;

    const HdrConfig cfg = HdrConfig::LoadFromFile(HdrConfig::DefaultPath());

    int rc = hdr_merge::HdrMergeFromFiles(lowPath, normalPath, highPath, outDir, cfg);
    std::cout << "HdrMerge return code = " << rc << std::endl;
    std::cout << "========== HdrMerge end ============" << std::endl;
}

// ============================================================
// 多档位对比：遍历 qualityPresets 里的所有 key，每档跑一次
//   - 如果 JSON 里加了 "3" / "4"，也会自动跑
// ============================================================
inline void RunHdrMergeMultiLevelTest()
{
    const std::string dataDir    = EnvRootPath() + "/data/HdrMerge/";
    const std::string lowPath    = dataDir + "low.jpg";
    const std::string normalPath = dataDir + "normal.jpg";
    const std::string highPath   = dataDir + "high.jpg";

    std::cout << "========== HdrMerge Multi-Level begin ==========" << std::endl;

    const HdrConfig baseCfg = HdrConfig::LoadFromFile(HdrConfig::DefaultPath());

    if (baseCfg.qualityPresets.empty())
    {
        std::cerr << "[HdrMergeMultiLevel] No qualityPresets defined, skip" << std::endl;
        return;
    }

    for (const auto& [key, preset] : baseCfg.qualityPresets)
    {
        int level = 0;
        try
        {
            level = std::stoi(key);
        }
        catch (...)
        {
            std::cerr << "[HdrMergeMultiLevel] Skip non-integer key: " << key << std::endl;
            continue;
        }

        std::cout << "\n--- [qualityLevel=" << level
                  << " / key=\"" << key << "\"] ---" << std::endl;

        HdrConfig cfg = baseCfg;
        cfg.qualityLevel = level;
        cfg.ApplyQualityLevel();

        const std::string outDir = dataDir + "result_level_" + key + "/";
        int rc = hdr_merge::HdrMergeFromFiles(lowPath, normalPath, highPath, outDir, cfg);
        std::cout << "[" << key << "] rc = " << rc << std::endl;
    }

    std::cout << "========== HdrMerge Multi-Level end ============" << std::endl;
}

#endif // HDR_MERGE_TEST_H
#ifndef HDR_MERGE_TEST_H
#define HDR_MERGE_TEST_H

#include "HdrMerge/HdrMerge.h"

inline void RunHdrMergeTest()
{
    const std::string dataDir = EnvRootPath() + "/data/HdrMerge/";

    const std::string lowPath    = dataDir + "low.jpg";
    const std::string normalPath = dataDir + "normal.jpg";
    const std::string highPath   = dataDir + "high.jpg";
    const std::string outDir     = dataDir + "result/";

    std::cout << "========== HdrMerge begin ==========" << std::endl;
    std::cout << "[HdrMerge] lowPath    = " << lowPath    << std::endl;
    std::cout << "[HdrMerge] normalPath = " << normalPath << std::endl;
    std::cout << "[HdrMerge] highPath   = " << highPath   << std::endl;
    std::cout << "[HdrMerge] outDir     = " << outDir     << std::endl;

    int rc = hdr_merge::HdrMergeFromFiles(lowPath, normalPath, highPath, outDir);
    std::cout << "HdrMerge return code = " << rc << std::endl;
    std::cout << "========== HdrMerge end ============" << std::endl;
}

#endif // HDR_MERGE_TEST_H
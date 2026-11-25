#ifndef TESTS_LIVEPHOTODECODE_LIVEPHOTODECODETEST_H
#define TESTS_LIVEPHOTODECODE_LIVEPHOTODECODETEST_H

#include <filesystem>
#include <iostream>

#include "LivePhotoDecode/LpDecode.h"
#include "LivePhotoDecode/LpDecodeDefine.h"
#include "LivePhotoDecode/LpMetaParser.h"

inline int RunLivePhotoDecodeTest()
{
    std::cout << "[LivePhotoDecode] input.jpg = " << LpInputJpgPath() << std::endl;
    std::cout << "[LivePhotoDecode] mp4      = " << LpInputMp4Path() << std::endl;
    std::cout << "[LivePhotoDecode] output   = " << LpOutputDir() << std::endl;

    if (!std::filesystem::exists(LpInputJpgPath())) {
        std::cerr << "[LivePhotoDecode] cover JPG missing: " << LpInputJpgPath() << std::endl;
        return 1;
    }
    if (!std::filesystem::exists(LpInputMp4Path())) {
        std::cerr << "[LivePhotoDecode] video MP4 missing: " << LpInputMp4Path() << std::endl;
        return 1;
    }

    ExtractCoverFrame();
    ExtractCoverExif();
    ExtractLiveStream();
    ExtractFrameMeta();
    return 0;
}

#endif // TESTS_LIVEPHOTODECODE_LIVEPHOTODECODETEST_H

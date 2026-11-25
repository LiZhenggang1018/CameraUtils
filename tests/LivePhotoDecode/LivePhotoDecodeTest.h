#ifndef TESTS_LIVEPHOTODECODE_LIVEPHOTODECODETEST_H
#define TESTS_LIVEPHOTODECODE_LIVEPHOTODECODETEST_H

#include <filesystem>
#include <iostream>

#include "LivePhotoDecode/LpDecode.h"
#include "LivePhotoDecode/LpDecodeDefine.h"
#include "LivePhotoDecode/LpMetaParser.h"

inline void RunLivePhotoDecodeTest()
{
    std::cout << "[LivePhotoDecode] input.jpg = " << LpInputJpgPath() << std::endl;
    std::cout << "[LivePhotoDecode] mp4      = " << LpInputMp4Path() << std::endl;
    std::cout << "[LivePhotoDecode] output   = " << LpOutputDir() << std::endl;

    if (!std::filesystem::exists(LpInputJpgPath())) {
        std::cerr << "[LivePhotoDecode] 封面 JPG 不存在: " << LpInputJpgPath() << std::endl;
        return;
    }
    if (!std::filesystem::exists(LpInputMp4Path())) {
        std::cerr << "[LivePhotoDecode] 视频 MP4 不存在: " << LpInputMp4Path() << std::endl;
        return;
    }

    ExtractCoverFrame();
    ExtractCoverExif();
    ExtractLiveStream();
    ExtractFrameMeta();
}

#endif // TESTS_LIVEPHOTODECODE_LIVEPHOTODECODETEST_H

#ifndef INC_LIVEPHOTODECODE_LPDECODEDEFINE_H
#define INC_LIVEPHOTODECODE_LPDECODEDEFINE_H

#include <string>

#include "CommonEnv.h"

// ============================================================
// 路径常量：函数内 static，避免跨文件静态初始化顺序问题。
// 输入素材为 vivo X200S 拍摄的 LivePhoto：封面 JPEG + 配套 MP4。
// ============================================================
inline const std::string& LivePhotoDecodeDataPath()
{
    static const std::string s = EnvRootPath() + "/data/LivePhotoDecode/";
    return s;
}

inline const std::string& LpInputJpgPath()
{
    static const std::string s = LivePhotoDecodeDataPath() + "input.jpg";
    return s;
}

inline const std::string& LpInputMp4Path()
{
    static const std::string s = LivePhotoDecodeDataPath() + "IMG_20261125_112849.mp4";
    return s;
}

inline const std::string& LpOutputDir()
{
    static const std::string s = LivePhotoDecodeDataPath() + "output";
    return s;
}

#endif // INC_LIVEPHOTODECODE_LPDECODEDEFINE_H

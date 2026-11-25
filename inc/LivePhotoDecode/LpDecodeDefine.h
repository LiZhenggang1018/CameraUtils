#ifndef INC_LIVEPHOTODECODE_LPDECODEDEFINE_H
#define INC_LIVEPHOTODECODE_LPDECODEDEFINE_H

#include <string>

#include "CommonEnv.h"

// ============================================================
// Path constants: function-local static to avoid cross-file static
// initialization order problems. Input is a vivo X200S LivePhoto:
// cover JPEG + companion MP4.
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

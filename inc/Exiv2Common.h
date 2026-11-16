#ifndef EXIV2COMMON_H
#define EXIV2COMMON_H

#include <string>
#include <unordered_map>
#include <exiv2/exiv2.hpp>
#include "CommonEnv.h"

// ============================================================
// 路径常量：函数内 static，避免跨文件静态初始化顺序问题
// ============================================================
inline const std::string& Exif2Path() {
    static const std::string s = EnvRootPath() + "/data/HdrMerge/normal.jpg";
    return s;
}

namespace CameraUtils {

// ============================================================
// 曝光信息：时间 + ISO
//   - 二者都有效才认为 valid()，物理 HDR 需要两者共同确定等效曝光量
// ============================================================
struct ExposureInfo
{
    double timeSeconds = -1.0;   // 秒
    double iso         = -1.0;
    bool valid() const { return timeSeconds > 0.0 && iso > 0.0; }
};

// ============================================================
// 从 EXIF 读取曝光时间与 ISO
//   - ExposureTime: Exif.Photo.ExposureTime (Rational，如 1/500 → 0.002)
//   - ISO:          Exif.Photo.PhotographicSensitivity (tag 34855)
//                   回退 Exif.Photo.ISOSpeedRatings
//   - 任一读取失败 → 保持 -1，valid() == false
// ============================================================
inline ExposureInfo ReadExposureInfo(const std::string& filePath)
{
    ExposureInfo info;
    try
    {
        auto image = Exiv2::ImageFactory::open(filePath);
        if (!image.get())
        {
            return info;
        }
        image->readMetadata();
        const auto& exif = image->exifData();
        if (exif.empty())
        {
            return info;
        }

        auto itTime = exif.findKey(Exiv2::ExifKey("Exif.Photo.ExposureTime"));
        if (itTime != exif.end())
        {
            info.timeSeconds = static_cast<double>(itTime->toFloat());
        }

        auto itIso = exif.findKey(Exiv2::ExifKey("Exif.Photo.PhotographicSensitivity"));
        if (itIso == exif.end())
        {
            itIso = exif.findKey(Exiv2::ExifKey("Exif.Photo.ISOSpeedRatings"));
        }
        if (itIso != exif.end())
        {
            info.iso = static_cast<double>(itIso->toFloat());
        }
    }
    catch (const Exiv2::Error&)
    {
        // 保持无效状态，由调用方决定回退策略
    }
    return info;
}

} // namespace CameraUtils

#endif // EXIV2COMMON_H
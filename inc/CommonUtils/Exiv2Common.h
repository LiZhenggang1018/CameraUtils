#ifndef INC_EXIV2COMMON_H
#define INC_EXIV2COMMON_H

#include <exiv2/exiv2.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include "CommonEnv.h"

// ============================================================
// 路径常量：函数内 static，避免跨文件静态初始化顺序问题
// ============================================================
inline const std::string& Exif2Path()
{
    static const std::string s = EnvRootPath() + "/data/HdrMerge/normal.jpg";
    return s;
}

namespace CameraUtils {

// ============================================================
// 曝光信息：时间 + ISO
//   - 二者都有效才认为 valid()，物理 HDR 需要两者共同确定等效曝光量
// ============================================================
struct ExposureInfo {
    double timeSeconds = -1.0; // 秒
    double iso = -1.0;
    [[nodiscard]] bool Valid() const { return timeSeconds > 0.0 && iso > 0.0; }
};

// ============================================================
// 从 EXIF 读取 ISO。
//   - 依次尝试 PhotographicSensitivity / ISOSpeedRatings /
//     RecommendedExposureIndex；逐个 key 独立 try，避免任一 key
//     在该 exiv2 版本下抛异常导致后续回退被跳过。
//   - 全部失败 → -1
// ============================================================
inline double ReadIso(const Exiv2::ExifData& exif)
{
    static const char* const kIsoKeys[] = {
        "Exif.Photo.PhotographicSensitivity",
        "Exif.Photo.ISOSpeedRatings",
        "Exif.Photo.RecommendedExposureIndex",
    };
    for (const char* key : kIsoKeys) {
        try {
            auto it = exif.findKey(Exiv2::ExifKey(key));
            if (it != exif.end()) {
                return static_cast<double>(it->toFloat());
            }
        } catch (const Exiv2::Error&) {
            // 该 key 在此 exiv2 版本不可用，继续尝试下一个
        }
    }
    return -1.0;
}

// ============================================================
// 从 EXIF 读取曝光时间与 ISO
//   - ExposureTime: Exif.Photo.ExposureTime (Rational，如 1/500 → 0.002)
//   - ISO:          ReadIso()（多 key 回退）
//   - 任一读取失败 → 保持 -1，valid() == false
// ============================================================
inline ExposureInfo ReadExposureInfo(const std::string& filePath)
{
    ExposureInfo info;
    try {
        auto image = Exiv2::ImageFactory::open(filePath);
        if (!image.get()) {
            return info;
        }
        image->readMetadata();
        const auto& exif = image->exifData();
        if (exif.empty()) {
            return info;
        }

        auto itTime = exif.findKey(Exiv2::ExifKey("Exif.Photo.ExposureTime"));
        if (itTime != exif.end()) {
            info.timeSeconds = static_cast<double>(itTime->toFloat());
        }

        info.iso = ReadIso(exif);
    } catch (const Exiv2::Error& e) {
        // 保持无效状态，由调用方决定回退策略
        (void)e;
    }
    return info;
}

} // namespace CameraUtils

#endif // INC_EXIV2COMMON_H
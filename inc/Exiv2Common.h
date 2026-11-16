#ifndef EXIV2COMMON_H
#define EXIV2COMMON_H

#include <string>
#include <unordered_map>
#include <exiv2/exiv2.hpp>

namespace CameraUtils {

inline std::unordered_map<std::string, std::string>
ReadExif(const std::string& filePath) {
    std::unordered_map<std::string, std::string> result;

    auto image = Exiv2::ImageFactory::open(filePath);
    if (!image.get()) {
        return result;
    }

    image->readMetadata();
    Exiv2::ExifData& exifData = image->exifData();
    if (exifData.empty()) {
        return result;
    }

    for (const auto& entry : exifData) {
        result[entry.key()] = entry.value().toString();
    }
    return result;
}

// 按需取单个常用字段的便捷函数
inline std::string GetExifValue(const std::string& filePath,
                                const std::string& key) {
    auto image = Exiv2::ImageFactory::open(filePath);
    if (!image.get()) {
        return {};
    }
    image->readMetadata();
    auto& exifData = image->exifData();
    auto it = exifData.findKey(Exiv2::ExifKey(key));
    if (it == exifData.end()) {
        return {};
    }
    return it->value().toString();
}

} // namespace CameraUtils

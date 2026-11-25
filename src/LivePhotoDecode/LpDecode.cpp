#include "LivePhotoDecode/LpDecode.h"

#include <fstream>
#include <iostream>

#include "Exiv2Common.h"
#include "LivePhotoSmoothPlay/DecAndEnc.h"
#include "LivePhotoSmoothPlay/YUVImageReader.h"
#include "OpenCVCommon.h"

namespace fs = std::filesystem;

void ExtractCoverFrame()
{
    const std::string& jpgPath = LpInputJpgPath();
    const std::string& outDir = LpOutputDir();

    std::error_code ec;
    if (!fs::exists(outDir) && !fs::create_directories(outDir, ec)) {
        std::cerr << "[ExtractCoverFrame] cannot create output dir: " << outDir << std::endl;
        return;
    }

    cv::Mat img = cv::imread(jpgPath, cv::IMREAD_COLOR);
    if (img.empty()) {
        std::cerr << "[ExtractCoverFrame] cannot read cover: " << jpgPath << std::endl;
        return;
    }

    const std::string outJpg = outDir + "/cover.jpg";
    const std::string outYuv = outDir + "/cover.yuv";

    if (!cv::imwrite(outJpg, img)) {
        std::cerr << "[ExtractCoverFrame] write JPG failed: " << outJpg << std::endl;
        return;
    }

    try {
        YUVImageReader reader(img.cols, img.rows);
        reader.SaveAsYuv420P(img, outYuv);
    } catch (const std::exception& e) {
        std::cerr << "[ExtractCoverFrame] write YUV failed: " << e.what() << std::endl;
        return;
    }

    std::cout << "[ExtractCoverFrame] " << img.cols << "x" << img.rows << " -> cover.jpg / cover.yuv" << std::endl;
}

void ExtractCoverExif()
{
    const std::string& jpgPath = LpInputJpgPath();
    const std::string& outDir = LpOutputDir();
    const std::string outTxt = outDir + "/cover_exif.txt";

    std::error_code ec;
    if (!fs::exists(outDir) && !fs::create_directories(outDir, ec)) {
        std::cerr << "[ExtractCoverExif] cannot create output dir: " << outDir << std::endl;
        return;
    }

    const CameraUtils::ExposureInfo expInfo = CameraUtils::ReadExposureInfo(jpgPath);

    // Read ISO directly: vivo stores ISO in ISOSpeedRatings (ReadExposureInfo
    // returns -1 because PhotographicSensitivity throws on this exiv2 build and
    // the exception is swallowed by its catch).
    double iso = -1.0;
    std::string dateTimeOriginal;
    try {
        auto image = Exiv2::ImageFactory::open(jpgPath);
        if (image.get()) {
            image->readMetadata();
            const auto& exif = image->exifData();

            auto itIso = exif.findKey(Exiv2::ExifKey("Exif.Photo.ISOSpeedRatings"));
            if (itIso == exif.end()) {
                itIso = exif.findKey(Exiv2::ExifKey("Exif.Photo.RecommendedExposureIndex"));
            }
            if (itIso != exif.end()) {
                iso = static_cast<double>(itIso->toFloat());
            }

            auto it = exif.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"));
            if (it != exif.end()) {
                dateTimeOriginal = it->value().toString();
            }
        }
    } catch (const Exiv2::Error& e) {
        std::cerr << "[ExtractCoverExif] read EXIF failed: " << e.what() << std::endl;
    }

    std::ofstream ofs(outTxt);
    if (!ofs.is_open()) {
        std::cerr << "[ExtractCoverExif] cannot write file: " << outTxt << std::endl;
        return;
    }

    ofs << "ExposureTime=" << expInfo.timeSeconds << "\n";
    ofs << "ISO=" << iso << "\n";
    ofs << "DateTimeOriginal=" << dateTimeOriginal << "\n";
    ofs << "\n[AllExifEntries]\n";

    try {
        auto image = Exiv2::ImageFactory::open(jpgPath);
        if (image.get()) {
            image->readMetadata();
            for (const auto& entry : image->exifData()) {
                ofs << entry.key() << " = " << entry.value().toString() << "\n";
            }
        }
    } catch (const Exiv2::Error& e) {
        std::cerr << "[ExtractCoverExif] dump all EXIF failed: " << e.what() << std::endl;
    }

    std::cout << "[ExtractCoverExif] exposure=" << expInfo.timeSeconds << "s, iso=" << iso
              << ", datetime=" << dateTimeOriginal << std::endl;
}

void ExtractLiveStream()
{
    const std::string& mp4Path = LpInputMp4Path();
    const std::string outDir = LpOutputDir() + "/video";

    std::error_code ec;
    if (!fs::exists(outDir) && !fs::create_directories(outDir, ec)) {
        std::cerr << "[ExtractLiveStream] cannot create output dir: " << outDir << std::endl;
        return;
    }

    const int rc = DecodeMp4ToYuvFrames(mp4Path.c_str(), outDir);
    if (rc != 0) {
        std::cerr << "[ExtractLiveStream] decode failed, rc " << rc << std::endl;
        return;
    }

    std::cout << "[ExtractLiveStream] decode done -> " << outDir << std::endl;
}

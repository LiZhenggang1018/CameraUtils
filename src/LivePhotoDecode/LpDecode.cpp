#include "LivePhotoDecode/LpDecode.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <vector>

#include "Exiv2Common.h"
#include "LivePhotoSmoothPlay/DecAndEnc.h"
#include "LivePhotoSmoothPlay/YUVImageReader.h"
#include "OpenCVCommon.h"

namespace fs = std::filesystem;

namespace {

// Read the raw cover JPEG bytes: input.jpg is a vivo LivePhoto container whose
// trailing proprietary metadata sits after the final EOI (FF D9). Copying up to
// and including that final EOI yields a byte-for-byte lossless cover JPEG that
// preserves the original EXIF (unlike imread+imwrite, which re-encodes lossily).
bool ReadCoverJpegBytes(const std::string& jpgPath, std::vector<uint8_t>& out)
{
    std::ifstream ifs(jpgPath, std::ios::binary);
    if (!ifs.is_open()) {
        return false;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (bytes.size() < 2) {
        return false;
    }

    // Find the last EOI (FF D9); the cover ends there.
    size_t lastEoi = std::numeric_limits<size_t>::max();
    for (size_t i = 0; i + 1 < bytes.size(); ++i) {
        if (bytes[i] == 0xFF && bytes[i + 1] == 0xD9) {
            lastEoi = i + 1; // index of the D9 byte
        }
    }
    if (lastEoi == std::numeric_limits<size_t>::max()) {
        return false;
    }

    out.assign(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(lastEoi + 1));
    return true;
}

} // namespace

void ExtractCoverFrame()
{
    const std::string& jpgPath = LpInputJpgPath();
    const std::string& outDir = LpOutputDir();

    std::error_code ec;
    if (!fs::exists(outDir) && !fs::create_directories(outDir, ec)) {
        std::cerr << "[ExtractCoverFrame] cannot create output dir: " << outDir << std::endl;
        return;
    }

    // cover.jpg: lossless byte-for-byte copy of the JPEG portion (EXIF preserved).
    std::vector<uint8_t> coverBytes;
    if (!ReadCoverJpegBytes(jpgPath, coverBytes)) {
        std::cerr << "[ExtractCoverFrame] cannot extract cover JPEG bytes: " << jpgPath << std::endl;
        return;
    }

    const std::string outJpg = outDir + "/cover.jpg";
    const std::string outYuv = outDir + "/cover.yuv";

    std::ofstream jpgOfs(outJpg, std::ios::binary);
    if (!jpgOfs.is_open()) {
        std::cerr << "[ExtractCoverFrame] cannot write file: " << outJpg << std::endl;
        return;
    }
    jpgOfs.write(reinterpret_cast<const char*>(coverBytes.data()), static_cast<std::streamsize>(coverBytes.size()));

    // cover.yuv: decode the cover and convert to YUV420P.
    cv::Mat img = cv::imread(outJpg, cv::IMREAD_COLOR);
    if (img.empty()) {
        std::cerr << "[ExtractCoverFrame] cannot decode cover: " << outJpg << std::endl;
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

    double iso = -1.0;
    std::string dateTimeOriginal;
    try {
        auto image = Exiv2::ImageFactory::open(jpgPath);
        if (image.get()) {
            image->readMetadata();
            const auto& exif = image->exifData();

            iso = CameraUtils::ReadIso(exif);

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

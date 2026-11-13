/*
 * @Description: FFMpeg公用定义头文件
 */

#ifndef FFMPEGDEFINE_H_
#define FFMPEGDEFINE_H_

#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>

#include "AllUsedHeadFiles.h"

#include "FFMpegCommon.h"
#include "CustomLog.h"
#include "CheckUtils.h"
#include "NoCopyableMacro.h"
#include "Singleton.h"
#include "ScopedUtils.h"
#include "CommonEnv.h"

// ============================================================
// 路径常量：全部改为"函数内 static"返回，避免跨文件静态初始化顺序问题
// 使用方式与原全局变量完全一致（通过 #define 宏别名），引用点无需修改
// ============================================================

// 根路径（末尾带斜杠，与原 LP_SMOOTH_PLAY_DATA_PATH 语义一致）
inline const std::string& LpSmoothPlayDataPath() {
    static const std::string s = EnvRootPath() + "/data/LivePhotoSmoothPlay/";
    return s;
}
#define LP_SMOOTH_PLAY_DATA_PATH (LpSmoothPlayDataPath())

// ---------- 视频文件 ----------
inline const std::string& InFileImpl()      { static const std::string s = LpSmoothPlayDataPath() + "input.mp4";     return s; }
inline const std::string& OutFileImpl()     { static const std::string s = LpSmoothPlayDataPath() + "output.mp4";    return s; }
inline const std::string& Out2FileImpl()    { static const std::string s = LpSmoothPlayDataPath() + "output2.mp4";   return s; }
inline const std::string& SLyfFileImpl()    { static const std::string s = LpSmoothPlayDataPath() + "smallLyf.mp4";  return s; }
inline const std::string& BLyfFileImpl()    { static const std::string s = LpSmoothPlayDataPath() + "bigLyf.mp4";    return s; }
inline const std::string& OriFileImpl()     { static const std::string s = LpSmoothPlayDataPath() + "ori.mp4";       return s; }
inline const std::string& OptFileImpl()     { static const std::string s = LpSmoothPlayDataPath() + "opt.mp4";       return s; }
inline const std::string& LpInitFileImpl()  { static const std::string s = LpSmoothPlayDataPath() + "Lp.mp4";        return s; }
inline const std::string& LpOptFileImpl()   { static const std::string s = LpSmoothPlayDataPath() + "LpOpt.mp4";     return s; }
inline const std::string& LpCropFileImpl()  { static const std::string s = LpSmoothPlayDataPath() + "LpCrop.mp4";    return s; }

inline const std::string& REG_IMG_CUR_FILE()
{
    static const std::string s = LpSmoothPlayDataPath() + "2.jpg";
    return s;
}
inline const std::string& REG_IMG_REF_FILE()
{
    static const std::string s = LpSmoothPlayDataPath() + "1.jpg";
    return s;
}

#define IN_FILE      (InFileImpl())
#define OUT_FILE     (OutFileImpl())
#define OUT2_FILE    (Out2FileImpl())
#define S_LYF_FILE   (SLyfFileImpl())
#define B_LYF_FILE   (BLyfFileImpl())
#define ORI_FILE     (OriFileImpl())
#define OPT_FILE     (OptFileImpl())
#define LP_INIT_FILE (LpInitFileImpl())
#define LP_OPT_FILE  (LpOptFileImpl())
#define LP_CROP_FILE (LpCropFileImpl())

// ---------- YUV 目录 ----------
inline const std::string& LpDecodeYuvDirImpl() {
    static const std::string s = LpSmoothPlayDataPath() + "LpJpg";
    return s;
}

inline const std::string& CropCoverDirImpl() {
    static const std::string s = LpSmoothPlayDataPath() + "CropCover";   // 去掉前导 /
    return s;
}

#define LP_DECODE_YUV_DIR (LpDecodeYuvDirImpl())
#define CROP_COVER_DIR    (CropCoverDirImpl())

// ---------- 查表（纯字面量，无跨全局依赖，保留原样） ----------
inline const std::unordered_map<std::string, int> VID_FILE_TO_LAST_VID_FRAME_IDX = {
    {"lyf", 191},
    {"cat", 275},
    {"cat&girl", 255},
    { "car", 303 }
};

// ---------- 编译期常量（不需要改） ----------
inline constexpr int LAST_VID_FRAME_IDX = 275;
inline constexpr int TransFrameNum = 20;

inline constexpr int YUV_W = 720;
inline constexpr int YUV_H = 1584;

inline constexpr int COVER_FRAME_IDX         = LAST_VID_FRAME_IDX + 21;
inline constexpr int TO_REPLACE_YUV_START_IDX = LAST_VID_FRAME_IDX + 1;
inline constexpr int TO_REPLACE_YUV_END_IDX   = COVER_FRAME_IDX - 1;

// ---------- YUV 帧文件（依赖 LP_DECODE_YUV_DIR 和上面常量） ----------
inline const std::string& LastVidFrameYuvImpl() {
    static const std::string s = LpDecodeYuvDirImpl() + "/" + std::to_string(LAST_VID_FRAME_IDX) + ".yuv";
    return s;
}
inline const std::string& CoverFrameYuvImpl() {
    static const std::string s = LpDecodeYuvDirImpl() + "/" + std::to_string(COVER_FRAME_IDX) + ".yuv";
    return s;
}

#define LAST_VID_FRAME_YUV (LastVidFrameYuvImpl())
#define COVER_FRAME_YUV    (CoverFrameYuvImpl())

// ---------- 其他路径 ----------
inline const std::string& RegResultDirImpl() {
    static const std::string s = LpSmoothPlayDataPath() + "coverToVidRegResult";
    return s;
}
inline const std::string& VidToCoverScaleRatioFileImpl() {
    static const std::string s = LpSmoothPlayDataPath() + "vidToCoverScaleRatio.txt";
    return s;
}
inline const std::string& CropRegResultDirImpl() {
    static const std::string s = LpSmoothPlayDataPath() + "cropReg/cropReg";
    return s;
}
inline const std::string& YuvWhFileImpl() {
    static const std::string s = LpSmoothPlayDataPath() + "yuvWidthHeight.txt";
    return s;
}

#define REG_RESULT_DIR            (RegResultDirImpl())
#define vidToCoverScaleRatioFile  (VidToCoverScaleRatioFileImpl())
#define CROP_REG_RESULT_DIR       (CropRegResultDirImpl())
#define YUV_WH_FILE               (YuvWhFileImpl())

// ============================================================
// 自定义删除器（与原文件一致）
// ============================================================
struct AVFormatContextDeleter
{
    void operator()(AVFormatContext* p) const noexcept
    {
        if (p)
            avformat_close_input(&p);
    }
};

struct AVCodecContextDeleter
{
    void operator()(AVCodecContext* p) const noexcept
    {
        if (p)
            avcodec_free_context(&p);
    }
};

struct AVPacketDeleter
{
    void operator()(AVPacket* p) const noexcept
    {
        if (p)
            av_packet_free(&p);
    }
};

struct AVFrameDeleter
{
    void operator()(AVFrame* p) const noexcept
    {
        if (p)
            av_frame_free(&p);
    }
};

struct SwsContextDeleter
{
    void operator()(SwsContext* p) const noexcept
    {
        if (p)
            sws_freeContext(p);
    }
};

struct FileDeleter
{
    void operator()(FILE* f) const noexcept
    {
        if (f)
            std::fclose(f);
    }
};

// 基于自定义删除器的智能指针类型别名
using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVCodecContextPtr  = std::unique_ptr<AVCodecContext,  AVCodecContextDeleter>;
using AVPacketPtr        = std::unique_ptr<AVPacket,        AVPacketDeleter>;
using AVFramePtr         = std::unique_ptr<AVFrame,         AVFrameDeleter>;
using SwsCtxPtr          = std::unique_ptr<SwsContext,      SwsContextDeleter>;
using FilePtr            = std::unique_ptr<FILE,            FileDeleter>;

#endif // FFMPEGDEFINE_H_
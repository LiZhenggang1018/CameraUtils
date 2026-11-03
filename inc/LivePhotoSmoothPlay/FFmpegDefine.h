/*
 * @Description: FFMpeg公用定义头文件
 */

#ifndef FFMPEGDEFINE_H_
#define FFMPEGDEFINE_H_

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

const std::string root = PROJECT_ROOT_DIR; // 必须使用此宏来获取项目根目录，确保在不同环境下路径正确

inline const std::string DATA_PATH = root + "/data/LivePhotoSmoothPlay/";

inline const std::string IN_FILE = DATA_PATH + "input.mp4";
inline const std::string OUT_FILE = DATA_PATH + "output.mp4";
inline const std::string OUT2_FILE = DATA_PATH + "output2.mp4";

inline const std::string S_LYF_FILE = DATA_PATH + "smallLyf.mp4"; // 小FOV，作为视频帧
inline const std::string B_LYF_FILE = DATA_PATH + "bigLyf.mp4";	  // 大FOV，用于产生拍照帧 跟平滑过渡帧

inline const std::string ORI_FILE = DATA_PATH + "ori.mp4"; // 原始LP文件
inline const std::string OPT_FILE = DATA_PATH + "opt.mp4"; // 优化后LP文件

inline const std::string LP_INIT_FILE = DATA_PATH + "Lp.mp4"; // 随手拍的原始LP
inline const std::string LP_OPT_FILE = DATA_PATH + "LpOpt.mp4"; // 优化后的LP
inline const std::string LP_CROP_FILE = DATA_PATH + "LpCrop.mp4"; // 随手拍的原始LP


inline const std::string LP_DECODE_YUV_DIR =
    DATA_PATH + "LpJpg"; // 原始LP文件逐帧解码生成的yuv&jpg文件路径
inline const std::string CROP_COVER_DIR = DATA_PATH + "/CropCover";

// 当前仅用于记录用
inline const std::unordered_map<std::string, int> VID_FILE_TO_LAST_VID_FRAME_IDX = {
	{"lyf", 191},
	{"cat", 275},
	{"cat&girl", 255},
	{ "car", 303 }
};

//// 需手动设置的部分参数
//// 对于LYF.mp4
inline constexpr int LAST_VID_FRAME_IDX = 275;					// 手动查看视频最后一帧（LpJpg下逆序播放jpg找到照片水印消失的那一帧）是第几帧
inline constexpr int TransFrameNum = 20; // 新生成的过渡帧总数

//  for fullScreen LP
inline constexpr int YUV_W = 720; // 这块目前打桩，后续可以改成从yuvWidthHeight.txt动态获取
inline constexpr int YUV_H = 1584;

//// for cropped car LP
//inline constexpr int YUV_W = 720;
//inline constexpr int YUV_H = 960; // 1562;

inline constexpr int COVER_FRAME_IDX = LAST_VID_FRAME_IDX + 21; // 封面帧总是在最后视频帧+20+1帧的位置
inline constexpr int TO_REPLACE_YUV_START_IDX = LAST_VID_FRAME_IDX + 1;
inline constexpr int TO_REPLACE_YUV_END_IDX = COVER_FRAME_IDX - 1;

// inline const std::string LAST_VID_FRAME_YUV = LP_DECODE_YUV_DIR + std::string("/") + std::to_string(LAST_VID_FRAME_IDX) + ".yuv";
// inline const std::string COVER_FRAME_YUV = LP_DECODE_YUV_DIR + std::string("/") + "212.yuv"; // 191+21

inline const std::string LAST_VID_FRAME_YUV = LP_DECODE_YUV_DIR + std::string("/") + std::to_string(LAST_VID_FRAME_IDX) + ".yuv";
inline const std::string COVER_FRAME_YUV = LP_DECODE_YUV_DIR + std::string("/") + std::to_string(COVER_FRAME_IDX) + ".yuv"; // 275+21

inline const std::string REG_RESULT_DIR =
    DATA_PATH + "coverToVidRegResult"; // 封面帧与最后的视频帧的图像配准结果
inline const std::string vidToCoverScaleRatioFile = DATA_PATH + "vidToCoverScaleRatio.txt";

inline const std::string CROP_REG_RESULT_DIR =
    DATA_PATH + "cropReg/cropReg"; // 过渡帧（封面帧剪裁不同FOV）与最后的视频帧的图像配准结果

inline const std::string YUV_WH_FILE = DATA_PATH + "yuvWidthHeight.txt";

// 自定义删除器
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
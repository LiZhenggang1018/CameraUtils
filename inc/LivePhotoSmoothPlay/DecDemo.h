/*
 * @Description: 用于生成原始LP文件 以及 生成优化后的LP文件
 */

#ifndef DECDEMO_H_
#define DECDEMO_H_

#include "FFmpegDefine.h"
#include "FFMpegUtils.h"
#include "DecAndEnc.h"
#include "GenMp4.h"

// 从bigLyf.mp4 解码生成yuv序列，选择最后一帧60.yuv作为后续使用的封面帧（大FOV的封面帧）
inline void DecodeCoverMp4ToYuvs()
{
	const char *input_filename = B_LYF_FILE.c_str();
	DecodeMp4ToYuvFrames(input_filename);
}

inline void DecodeLpMp4ToYuvs()
{
	const char *input_filename = LP_INIT_FILE.c_str();
	const std::string outDir = LP_DECODE_YUV_DIR;
	DecodeMp4ToYuvFrames(input_filename, outDir);
}

inline void DecodeOptLpMp4ToYuvs()
{
	const char *input_filename = LP_OPT_FILE.c_str();
    const std::string outDir = LP_SMOOTH_PLAY_DATA_PATH + "LpOptJpg";
	DecodeMp4ToYuvFrames(input_filename, outDir);
}

// 生成小fov视频+一帧封面帧（大fov的60.yuv）在视频结尾。作为原始LP文件。
inline void GenOriginalLivePhoto()
{
	const char *input_filename = S_LYF_FILE.c_str();
	const char *output_filename = ORI_FILE.c_str(); // 小fov视频+一帧大fov在视频结尾（BigFovLyf/60.yuv 的一帧，作为视频的最后一帧）。
	DecodeEncodeAndInsertSingleYuv(input_filename, output_filename);
}

// 生成过渡帧优化后的LP文件：由 小fov视频+  一系列过渡帧（FOV逐渐变大，从视频帧Fov逐步增大到封面帧Fov） + 一帧封面帧 视频结尾。
inline void GenOptLivePhoto()
{

	const char *input_filename = S_LYF_FILE.c_str(); // 小fov视频
	const char *output_filename = OPT_FILE.c_str();	 // 生成小fov视频+  一系列过渡帧（FOV渐变的）。
	// "new1.yuv"  FOV最大 放到最后面
	// "new10.yuv"  FOV最小 放到最前面
	DecodeAndEncodeAndInsertMultiYuvs(input_filename, output_filename);
}

// 生成过渡帧优化后的LP文件：由 小fov视频+  Fov渐变过渡帧替代原始的alpha通道过渡帧 + 一帧封面帧 视频结尾。
// Lp.MP4 一共有1~247帧。191 最后视频帧; [192,211] AlphaMerge融合帧; 212视频帧
inline void ReplaceAlhpaMergeToFovSmoothLp()
{

	const char *input_filename = LP_INIT_FILE.c_str(); //
	const char *output_filename = LP_OPT_FILE.c_str(); //

	std::vector<std::string> replaceYuvFiles{};
	size_t length = TransFrameNum; // 先插入crop20 最后插入crop1；
	for (size_t i = length; i > 0; --i)
	{
		// crop1.yuv cropTransFrameNum.yuv
		auto name = CROP_COVER_DIR + std::string{"/crop"} + std::to_string(i) + ".yuv";
		replaceYuvFiles.push_back(name);
	}

	DecodeAndEncodeAndReplaceMultiYuvs(input_filename, output_filename, TO_REPLACE_YUV_START_IDX, TO_REPLACE_YUV_END_IDX, replaceYuvFiles);
}

// 生成过渡帧优化后的LP文件：由 小fov视频+  【Fov渐变 + alphaMerge过渡帧】替代原始的alpha通道过渡帧 + 一帧封面帧 视频结尾。
inline void ReplaceAlhpaMergeToFovSmoothAndAlpaMergedLp()
{

	const char* input_filename = LP_INIT_FILE.c_str(); //
	const char* output_filename = LP_OPT_FILE.c_str(); //

	std::vector<std::string> replaceYuvFiles{};
	size_t length = TransFrameNum;
	for (size_t i = length; i > 0; --i)
	{
		// crop1.yuv cropTransFrameNum.yuv
		auto name = CROP_COVER_DIR + std::string{ "/crop" } + std::to_string(i) + ".yuv";
		replaceYuvFiles.push_back(name);
	}

	DecodeAndEncodeAndReplaceMultiYuvs(input_filename, output_filename, TO_REPLACE_YUV_START_IDX, TO_REPLACE_YUV_END_IDX, replaceYuvFiles);
}

#endif // DECDEMO_H_

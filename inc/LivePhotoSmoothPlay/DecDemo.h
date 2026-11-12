/*
 * @Description: 用于生成原始LP文件 以及 生成优化后的LP文件
 */

#ifndef DECDEMO_H_
#define DECDEMO_H_

#include "FFmpegDefine.h"
#include "FFMpegUtils.h"
#include "DecAndEnc.h"

// 从bigLyf.mp4 解码生成yuv序列，选择最后一帧60.yuv作为后续使用的封面帧
inline void DecodeCoverMp4ToYuvs()
{
    DecodeMp4ToYuvFrames(B_LYF_FILE.c_str());
}

inline void DecodeLpMp4ToYuvs()
{
    DecodeMp4ToYuvFrames(LP_INIT_FILE.c_str(), LP_DECODE_YUV_DIR);
}

inline void DecodeOptLpMp4ToYuvs()
{
    const std::string outDir = LP_SMOOTH_PLAY_DATA_PATH + "LpOptJpg";
    DecodeMp4ToYuvFrames(LP_OPT_FILE.c_str(), outDir);
}

// 替换 alpha 混合过渡帧为 FOV 渐变过渡帧
// Lp.mp4: [1, LAST_VID_FRAME_IDX] 视频帧; [START, END] alpha混合帧; [COVER_FRAME_IDX] 封面帧
inline void ReplaceAlhpaMergeToFovSmoothLp()
{
    std::vector<std::string> replaceYuvFiles;
    replaceYuvFiles.reserve(TransFrameNum);
    // 逆序：crop20 → crop1（FOV 从宽到窄，对应从视频帧过渡到封面帧）
    for (int i = TransFrameNum; i >= 1; --i)
    {
        replaceYuvFiles.push_back(CROP_COVER_DIR + "/crop" + std::to_string(i) + ".yuv");
    }

    DecodeAndEncodeAndReplaceMultiYuvs(
        LP_INIT_FILE.c_str(),
        LP_OPT_FILE.c_str(),
        TO_REPLACE_YUV_START_IDX,
        TO_REPLACE_YUV_END_IDX,
        replaceYuvFiles);
}

// 修复 #13：删除 ReplaceAlhpaMergeToFovSmoothAndAlpaMergedLp
// （原函数与 ReplaceAlhpaMergeToFovSmoothLp 逐字相同，是未实现的占位）
// 如果将来要"FOV 渐变 + alpha 混合"的组合方案，应在此重新实现

#endif // DECDEMO_H_
/*
 * @Description: LpFovtTransitionOpt 头文件合集
 */

#ifndef LPFOVTRANSITIONOPT_H_
#define LPFOVTRANSITIONOPT_H_

//#include <windows.h> // 用于SetConsoleOutputCP

#include "DecDemo.h"
#include "OpenCVCommon.h"
#include "YUVProcessor.h"
#include "AlphaMergeYuvs.h"
#include "LpReg.h"
#include "CenterCropGenYuvs.h"

// void GenFrom2Mp4ToOriAndOptLp()
//{

//	//完整的demo流程：
//	DecodeCoverMp4ToYuvs(); // 1、从 bigLyf.mp4 解码生成
//yuv序列（1.yuv~60.yuv），后面我们选择最后一帧 60.yuv 作为封面帧 	ConvertCoverFrameToMultiFovYuvs();
//// 2、从封面帧生成一序列FOV渐变的过渡帧(new1.yuv new10.yuv)
////3、生成ori.mp4: 生成小fov视频+一帧大fov封面帧在视频结尾（60.yuv）。
//4、生成opt.mp4: 生成小fov视频+  一系列过渡帧（FOV渐变的） + 一帧大fov封面帧 在视频结尾。
//}

inline void AuxTestDumpUtils()
{
    // DecodeCropEncodeInputVid(); // 对原始Lp视频进行5%中心裁剪，生成裁剪后的视频LpCrop.mp4
    // TestDumpUtils();
    //  TestAlphaMergeYuvsFromFiles();
    //  TestAlphaMergeYuvs();
}

// 从原始Lp文件生成过渡帧优化后的Lp文件。
inline void GenFromLpToOptLp()
{
    DecodeLpMp4ToYuvs(); // 1、从 Lp.mp4 解码生成 yuv jpg序列（1.yuv~N.yuv）
    // 1.5、
    // 这里需暂停手动找一下视频的最后一帧（赋值给LAST_VID_FRAME_IDX）和封面帧（COVER_FRAME_IDX）分别是哪两帧
    // yuv 图像宽高确保都是已知
    // 注意：这里是采用的录屏，X300U录屏工具生成的.mp4文件一般为90FPS，即1s视频有90帧

    RegCoverToVid();                // 2、对封面帧进行配准，使其和视频最后一帧对齐
    GenDiffFovYuvsFromCoverFrame(); // 3、基于封面帧生成20帧crop帧 crop1.yuv
    // 直接把crop1~N.yuv 替换掉旧的alpha通道过渡帧就行了。
    // crop帧 crop1.yuv 配准到视频帧。
    ReplaceAlhpaMergeToFovSmoothLp(); // 4、生成LpOpt.mp4: 生成小fov视频+  一系列过渡帧（FOV渐变的）
                                      // + 一帧大fov封面帧 在视频结尾。
    DecodeOptLpMp4ToYuvs();
}

inline void LpFovTransitionOptTest()
{
    // AuxTestDumpUtils();
    GenFromLpToOptLp();
}

#endif // LPFOVTRANSITIONOPT_H_
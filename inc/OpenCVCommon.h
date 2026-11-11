#ifndef OPENCV_COMMON_H
#define OPENCV_COMMON_H

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/video/tracking.hpp>   // ← 新增：DISOpticalFlow 在此头文件中
#include <opencv2/optflow.hpp>          // 保留：如果你还在用 optflow 模块的其他算法
#include <opencv2/ximgproc.hpp>

#include <AllUsedHeadFiles.h>


#endif // OPENCV_COMMON_H
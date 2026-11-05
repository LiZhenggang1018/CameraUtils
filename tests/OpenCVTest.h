#ifndef OPENCVTEST_H
#define OPENCVTEST_H

#include <iostream>
#include <string>
#include "OpenCVCommon.h"

using namespace cv;

inline const std::string PicPath = std::string(PROJECT_ROOT_DIR) + "/data/lhc.jpg";

inline void OpenCVTest()
{
    {
        HMODULE h = GetModuleHandleA("opencv_world4140d.dll");
        if (h)
        {
            char buf[MAX_PATH] = {};
            GetModuleFileNameA(h, buf, MAX_PATH);
            std::cout << "Loaded OpenCV DLL: " << buf << std::endl;
        }
    }
    Mat test = imread(PicPath);
    imshow("test", test); // 运行时窗口有几行报错，可忽视。
    waitKey(); // 这个wait key是必须的，不然窗口图像显示不出来，哪怕system("pause")都不行
    cv::destroyAllWindows();
}

#endif // !OPENCVTEST_H

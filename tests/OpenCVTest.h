#ifndef OPENCVTEST_H
#define OPENCVTEST_H

#include <iostream>
#include <string>
#include "OpenCVCommon.h"

using namespace cv;

// 路径常量：改成"函数内 static"，避免跨文件静态初始化顺序问题
inline const std::string& PicPathImpl() {
    static const std::string s = std::string(PROJECT_ROOT_DIR) + "/data/lhc.jpg";
    return s;
}

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
    Mat test = imread(PicPathImpl());
    imshow("test", test); // 运行时窗口有几行报错，可忽视。
    waitKey(); // 这个wait key是必须的，不然窗口图像显示不出来，哪怕system("pause")都不行
    cv::destroyAllWindows();
}

#endif // !OPENCVTEST_H
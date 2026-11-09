项目名称：\
CameraUtils

项目简介：\
Utils code for Android camera HAL use / 安卓相机领域 硬件抽象层HAL的一些通用工具类+代码

运行环境：\
windows11 OS + visual studio 2022 本地计算机 x64Debug,

编程语言：\
C++20

构建依赖：\
cmake 构建,通过CPM下载3方依赖库：opencv(含contrib模块) ffmpeg stdexec nlohmann_json MagicEnum

个人备注：\
1. 手动下载rapids-cmake，解压到如下路径D:/ThirdPartyLib/rapids-cmake，解决内网CMake 在下载 rapids-cmake 时 SSL 验证失败问题：
2. 为了节省opencv-contrib每次清理后重新构建的耗时操作，在本地路径D:\ThirdPartyLib\OpenCvForCameraUtils\install 安装了OpenCV-contrib的debug与release

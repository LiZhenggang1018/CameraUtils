# CMakeList.txt: CommonHeads 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.15)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project(CameraUtilsProject LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# 包含 FetchContent 模块
include(FetchContent)

set(FETCHCONTENT_SOURCE_DIR_RAPIDS-CMAKE "D:/ThirdPartyLib/rapids-cmake")


# 声明 stdexec 依赖项
FetchContent_Declare(
    stdexec
    GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
    GIT_TAG        main  # 建议固定到具体的 commit 或 tag 以保证构建稳定性
    GIT_SHALLOW    TRUE  # 浅克隆，加速下载
)

# 可选：关闭 stdexec 自带的示例和测试，加速配置过程
set(STDEXEC_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(STDEXEC_BUILD_TESTS    OFF CACHE BOOL "" FORCE)

# 使 stdexec 可用
FetchContent_MakeAvailable(stdexec)

find_package(OpenCV REQUIRED COMPONENTS
core 
imgproc 
highgui
features2d
calib3d
flann
video
imgcodecs
objdetect
)
message(STATUS "OpenCV_INCLUDE_DIRS = ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV_LIBS         = ${OpenCV_LIBS}")

# ---------- FFmpeg ----------
# FFMPEG_ROOT 由 CMakePresets.json 注入；若未注入则给默认值
if(NOT DEFINED FFMPEG_ROOT)
    set(FFMPEG_ROOT "D:/ThirdPartyLib/ffmpeg/ffmpeg_windows/ffmpeg-master-latest-win64-gpl-shared")
endif()

set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR     "${FFMPEG_ROOT}/lib")

# 需要的 FFmpeg 模块（按实际使用增减）
set(FFMPEG_LIBS
    avcodec
    avformat
    avutil
    swscale
    swresample
    avfilter
    avdevice
)

# 检查头文件和库目录是否存在，避免配置阶段静默失败
if(NOT EXISTS "${FFMPEG_INCLUDE_DIR}/libavcodec/avcodec.h")
    message(FATAL_ERROR "FFmpeg include not found: ${FFMPEG_INCLUDE_DIR}")
endif()
if(NOT EXISTS "${FFMPEG_LIB_DIR}")
    message(FATAL_ERROR "FFmpeg lib dir not found: ${FFMPEG_LIB_DIR}")
endif()

message(STATUS "FFMPEG_ROOT        = ${FFMPEG_ROOT}")
message(STATUS "FFMPEG_INCLUDE_DIR = ${FFMPEG_INCLUDE_DIR}")
message(STATUS "FFMPEG_LIB_DIR     = ${FFMPEG_LIB_DIR}")
# -----------------------------

# ---------- clang-tidy（可选） ----------
option(ENABLE_CLANG_TIDY "Enable clang-tidy checks" OFF)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    else()
        message(WARNING "ENABLE_CLANG_TIDY 已开启，但未找到 clang-tidy")
    endif()
endif()
# -------------------------------------------


# 开启测试功能
enable_testing()


# 先添加 src，因为 tests 中会引用 src 生成的可执行目标
add_subdirectory(src)

# 再添加 tests
add_subdirectory(tests)
# TODO: 如有需要，请添加测试并安装目标。

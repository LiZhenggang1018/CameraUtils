cmake_minimum_required (VERSION 3.15)

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

# ============================================================
# 通用函数：本地 + CI 双环境支持
# ============================================================
function(camera_utils_resolve_lib_root LIB_NAME ENV_VAR LOCAL_PATH)
    string(TOUPPER "${LIB_NAME}" _lib_upper)
    if(DEFINED ENV{${ENV_VAR}} AND EXISTS "$ENV{${ENV_VAR}}")
        file(TO_CMAKE_PATH "$ENV{${ENV_VAR}}" _resolved)
        message(STATUS "${LIB_NAME}: using env ${ENV_VAR} = ${_resolved}")
    elseif(EXISTS "${LOCAL_PATH}")
        set(_resolved "${LOCAL_PATH}")
        message(STATUS "${LIB_NAME}: using local path = ${_resolved}")
    else()
        message(FATAL_ERROR
            "${LIB_NAME} not found.\n"
            "  本地：确认 ${LOCAL_PATH} 存在\n"
            "  CI：确认 workflow 设置了 ${ENV_VAR} 环境变量，且路径下有该库的配置文件")
    endif()
    set(${_lib_upper}_ROOT "${_resolved}" PARENT_SCOPE)
endfunction()

function(camera_utils_copy_dlls TARGET_NAME SRC_DIR)
    if(EXISTS "${SRC_DIR}")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${SRC_DIR}"
                "$<TARGET_FILE_DIR:${TARGET_NAME}>"
            COMMENT "Copying DLLs from ${SRC_DIR} to $<TARGET_FILE_DIR:${TARGET_NAME}>")
    else()
        message(STATUS "Skip DLL copy for ${TARGET_NAME}: ${SRC_DIR} not found")
    endif()
endfunction()

# ============================================================
# CPM
# ============================================================
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
    message(FATAL_ERROR
        "cmake/CPM.cmake not found. It should be committed to the repo.\n"
        "Run: git add cmake/CPM.cmake && git commit")
endif()
set(CPM_SOURCE_CACHE "$ENV{USERPROFILE}/.cpm-cache" CACHE PATH "CPM source cache")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

# ============================================================
# rapids-cmake：本地优先，CI 自动下载
# ============================================================
if(EXISTS "D:/ThirdPartyLib/rapids-cmake")
    set(FETCHCONTENT_SOURCE_DIR_RAPIDS-CMAKE "D:/ThirdPartyLib/rapids-cmake")
    message(STATUS "Local rapids-cmake found, using: D:/ThirdPartyLib/rapids-cmake")
else()
    message(STATUS "Local rapids-cmake not found, will download automatically.")
    unset(FETCHCONTENT_SOURCE_DIR_RAPIDS-CMAKE CACHE)
endif()

# ============================================================
# nlohmann_json
# ============================================================
CPMAddPackage(
    NAME json
    URL      https://hf-mirror.com/csukuangfj/sherpa-ncnn-cmake-deps/resolve/main/json-3.12.0.tar.gz
    URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OPTIONS "JSON_BuildTests OFF"
)

# ============================================================
# stdexec
# ============================================================
CPMAddPackage(
    NAME stdexec
    GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    OPTIONS
        "STDEXEC_BUILD_EXAMPLES OFF"
        "STDEXEC_BUILD_TESTS OFF"
)

# ============================================================
# magic_enum
# ============================================================
CPMAddPackage(
    NAME magic_enum
    VERSION 0.9.8
    GITHUB_REPOSITORY Neargye/magic_enum
    OPTIONS "MAGIC_ENUM_OPT_INSTALL YES"
)

# ============================================================
# OpenCV + contrib（optflow）
# - 本地已有带 contrib 的安装 → 快速加载
# - 否则从源码构建（CI 必经）
# ============================================================
# 软检测候选路径（不 FATAL_ERROR，找不到就走源码构建）
if(DEFINED ENV{OPENCV_ROOT} AND EXISTS "$ENV{OPENCV_ROOT}")
    file(TO_CMAKE_PATH "$ENV{OPENCV_ROOT}" _opencv_candidate)
    message(STATUS "OpenCV: 候选路径来自环境变量 OPENCV_ROOT = ${_opencv_candidate}")
elseif(EXISTS "D:/ThirdPartyLib/OpenCvForCameraUtils/install")
    set(_opencv_candidate "D:/ThirdPartyLib/OpenCvForCameraUtils/install")
    message(STATUS "OpenCV: 候选路径为本地 install = ${_opencv_candidate}")
else()
    set(_opencv_candidate "")
    message(STATUS "OpenCV: 无本地候选路径")
endif()

# 检测候选路径下是否有 optflow 头文件
set(_opencv_local_has_contrib FALSE)
if(_opencv_candidate AND EXISTS "${_opencv_candidate}")
    file(GLOB_RECURSE _optflow_headers "${_opencv_candidate}/include/opencv2/optflow.hpp")
    if(_optflow_headers)
        set(_opencv_local_has_contrib TRUE)
    endif()
endif()

if(_opencv_local_has_contrib)
    # ---------- 分支 A：本地已安装带 contrib 的 OpenCV ----------
    message(STATUS "OpenCV: 使用本地带 contrib 的安装: ${_opencv_candidate}")
    CPMAddPackage(
        NAME opencv_prebuilt
        SOURCE_DIR "${_opencv_candidate}"
        DOWNLOAD_ONLY YES
    )
    set(OpenCV_DIR "" CACHE PATH "Path to OpenCVConfig.cmake" FORCE)
    file(GLOB_RECURSE _config_files "${_opencv_candidate}/OpenCVConfig.cmake")
    if(_config_files)
        list(GET _config_files 0 _first)
        get_filename_component(OpenCV_DIR "${_first}" DIRECTORY)
    endif()
    if(NOT OpenCV_DIR OR NOT EXISTS "${OpenCV_DIR}/OpenCVConfig.cmake")
        message(FATAL_ERROR "OpenCVConfig.cmake not found under ${_opencv_candidate}")
    endif()
    message(STATUS "OpenCV_DIR = ${OpenCV_DIR}")
find_package(OpenCV REQUIRED COMPONENTS
    core imgproc highgui features2d calib3d flann video imgcodecs objdetect optflow photo stitching xphoto ximgproc stereo)
    # ${OpenCV_LIBS} 和 ${OpenCV_INCLUDE_DIRS} 由 find_package 提供
else()
    # ---------- 分支 B：从源码构建 ----------
    message(STATUS "OpenCV: 本地无带 contrib 的安装，从源码构建（包含 optflow）")

    # 1) 先下载 contrib 源码（只下载不构建）
    CPMAddPackage(
        NAME opencv_contrib
        GITHUB_REPOSITORY opencv/opencv_contrib
        GIT_TAG        4.10.0
        GIT_SHALLOW    TRUE
        DOWNLOAD_ONLY  YES
    )

    # 2) 构建 OpenCV，挂上 contrib 的 modules 目录
    CPMAddPackage(
        NAME opencv
        GITHUB_REPOSITORY opencv/opencv
        GIT_TAG        4.10.0
        GIT_SHALLOW    TRUE
        OPTIONS
            "OPENCV_EXTRA_MODULES_PATH=${opencv_contrib_SOURCE_DIR}/modules"
            "BUILD_LIST=core,imgproc,highgui,features2d,calib3d,flann,video,imgcodecs,objdetect,optflow,photo,stitching,xphoto,ximgproc,stereo"
            "BUILD_TESTS=OFF"
            "BUILD_PERF_TESTS=OFF"
            "BUILD_EXAMPLES=OFF"
            "BUILD_opencv_apps=OFF"
            "BUILD_opencv_python2=OFF"
            "BUILD_opencv_python3=OFF"
            "BUILD_opencv_js=OFF"
            "BUILD_JAVA=OFF"
            "WITH_CUDA=OFF"
            "WITH_IPP=OFF"
            "BUILD_SHARED_LIBS=ON"
    )

    # 统一成与 find_package 分支相同的变量名，src/CMakeLists.txt 无需改
    set(OpenCV_LIBS
        opencv_core opencv_imgproc opencv_highgui opencv_features2d
        opencv_calib3d opencv_flann opencv_video opencv_imgcodecs
        opencv_objdetect opencv_optflow
    )
    set(OpenCV_INCLUDE_DIRS
        "${opencv_SOURCE_DIR}/modules/core/include"
        "${opencv_SOURCE_DIR}/modules/imgproc/include"
        "${opencv_SOURCE_DIR}/modules/highgui/include"
        "${opencv_SOURCE_DIR}/modules/features2d/include"
        "${opencv_SOURCE_DIR}/modules/calib3d/include"
        "${opencv_SOURCE_DIR}/modules/flann/include"
        "${opencv_SOURCE_DIR}/modules/video/include"
        "${opencv_SOURCE_DIR}/modules/imgcodecs/include"
        "${opencv_SOURCE_DIR}/modules/objdetect/include"
        "${opencv_contrib_SOURCE_DIR}/modules/optflow/include"
    )
    message(STATUS "OpenCV: 源码构建模式，target 列表 = ${OpenCV_LIBS}")
endif()

# ============================================================
# FFmpeg（CPM 下载预编译包）
# ============================================================
set(FFMPEG_PREBUILT_URL
    "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip")

CPMAddPackage(
    NAME ffmpeg_prebuilt
    URL  "${FFMPEG_PREBUILT_URL}"
    DOWNLOAD_ONLY YES
)
set(FFMPEG_ROOT "${ffmpeg_prebuilt_SOURCE_DIR}")

if(NOT EXISTS "${FFMPEG_ROOT}/include")
    file(GLOB _ffmpeg_candidates LIST_DIRECTORIES true "${FFMPEG_ROOT}/*")
    foreach(_cand IN LISTS _ffmpeg_candidates)
        if(IS_DIRECTORY "${_cand}" AND EXISTS "${_cand}/include")
            set(FFMPEG_ROOT "${_cand}")
            break()
        endif()
    endforeach()
endif()

set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR     "${FFMPEG_ROOT}/lib")
set(FFMPEG_LIBS avcodec avformat avutil swscale swresample avfilter avdevice)

if(NOT EXISTS "${FFMPEG_INCLUDE_DIR}/libavcodec/avcodec.h")
    message(FATAL_ERROR
        "FFmpeg include not found.\n"
        "  FFMPEG_ROOT        = ${FFMPEG_ROOT}\n"
        "  FFMPEG_INCLUDE_DIR = ${FFMPEG_INCLUDE_DIR}\n"
        "  ffmpeg_prebuilt_SOURCE_DIR = ${ffmpeg_prebuilt_SOURCE_DIR}")
endif()
if(NOT EXISTS "${FFMPEG_LIB_DIR}")
    message(FATAL_ERROR "FFmpeg lib dir not found: ${FFMPEG_LIB_DIR}")
endif()

message(STATUS "FFMPEG_ROOT        = ${FFMPEG_ROOT}")
message(STATUS "FFMPEG_INCLUDE_DIR = ${FFMPEG_INCLUDE_DIR}")
message(STATUS "FFMPEG_LIB_DIR     = ${FFMPEG_LIB_DIR}")

enable_testing()
add_subdirectory(src)
add_subdirectory(tests)
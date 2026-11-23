cmake_minimum_required (VERSION 3.15)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  # ASan（/fsanitize）与 EditAndContinue（/ZI）不兼容，此时改用 ProgramDatabase（/Zi）。
  if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase")
  else()
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
  endif()
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

# 为可执行目标复制 FFmpeg + 当前构建类型对应的 OpenCV 运行库
function(camera_utils_copy_runtime_dlls TARGET_NAME)
    camera_utils_copy_dlls(${TARGET_NAME} "${FFMPEG_ROOT}/bin")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # Debug：只要以 d.dll 结尾的（含分模块 opencv_core4100d.dll、opencv_imgproc4100d.dll 等）
        file(GLOB_RECURSE _opencv_dlls CONFIGURE_DEPENDS
            "${opencv_prebuilt_SOURCE_DIR}/opencv_*d.dll"
        )
    else()
        # Release：glob 全部 opencv_*.dll 后排除以 d.dll 结尾的 Debug 版
        file(GLOB_RECURSE _opencv_dlls CONFIGURE_DEPENDS
            "${opencv_prebuilt_SOURCE_DIR}/opencv_*.dll"
        )
        list(FILTER _opencv_dlls EXCLUDE REGEX "d\\.dll$")
    endif()

    if(_opencv_dlls)
        foreach(_dll IN LISTS _opencv_dlls)
            message(STATUS "Will copy OpenCV DLL: ${_dll}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_dll}"
                    "$<TARGET_FILE_DIR:${TARGET_NAME}>"
                COMMENT "Copying OpenCV DLL: ${_dll}")
        endforeach()
    else()
        message(WARNING "No OpenCV DLL found to copy under ${opencv_prebuilt_SOURCE_DIR}")
    endif()

    # ASan 运行时库：clang_rt.asan*.dll 在 MSVC 工具集 bin 目录下，运行时需能被找到。
    # 构建期复制到 exe 同目录，避免运行时手动设 PATH（否则启动报 0xC0000135）。
    if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
        get_filename_component(_clang_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        file(GLOB _asan_dlls CONFIGURE_DEPENDS "${_clang_bin_dir}/clang_rt.asan*.dll")
        foreach(_dll IN LISTS _asan_dlls)
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_dll}"
                    "$<TARGET_FILE_DIR:${TARGET_NAME}>"
                COMMENT "Copying ASan runtime DLL: ${_dll}")
        endforeach()
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

# ============================================================
# OpenCV 模块清单：
# - 只保留工程实际使用的模块，两个构建分支必须完全一致
#   core      Mat / Ptr
#   imgproc   cvtColor / warpAffine / warpPerspective
#   highgui   imshow / waitKey
#   imgcodecs imread / imwrite
#   video     calcOpticalFlow* / DISOpticalFlow / goodFeaturesToTrack
#   videoio   VideoCapture
#   features2d ORB / SIFT / BRISK / AKAZE / BFMatcher / drawMatches
#   calib3d   findHomography / estimateAffine2D
#   flann     FlannBasedMatcher
#   optflow   createOptFlow_DeepFlow        （contrib）
#   ximgproc  guidedFilter                  （contrib）
# ============================================================
set(CAMERA_UTILS_OPENCV_MODULES
    core imgproc highgui imgcodecs video videoio features2d calib3d flann photo)
set(CAMERA_UTILS_OPENCV_CONTRIB_MODULES
    optflow ximgproc)
set(CAMERA_UTILS_OPENCV_COMPONENTS
    ${CAMERA_UTILS_OPENCV_MODULES} ${CAMERA_UTILS_OPENCV_CONTRIB_MODULES})
# BUILD_LIST 要求逗号分隔
string(REPLACE ";" "," CAMERA_UTILS_OPENCV_BUILD_LIST "${CAMERA_UTILS_OPENCV_COMPONENTS}")

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
    # OpenCVConfig.cmake 仅在 MSVC_VERSION 匹配 ^19[34][0-9]$ 时才推断 vc17；
    # VS2026(MSVC 19.5x) 会推断失败并置 OpenCV_FOUND=FALSE，这里显式兜底。
    if(MSVC AND MSVC_VERSION GREATER_EQUAL 1950 AND NOT OpenCV_RUNTIME)
        set(OpenCV_RUNTIME vc17)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(OpenCV_ARCH x64)
        else()
            set(OpenCV_ARCH x86)
        endif()
        message(STATUS "OpenCV: 显式指定 runtime=${OpenCV_RUNTIME} arch=${OpenCV_ARCH} (MSVC ${MSVC_VERSION})")
    endif()
find_package(OpenCV REQUIRED COMPONENTS ${CAMERA_UTILS_OPENCV_COMPONENTS})
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
            "BUILD_LIST=${CAMERA_UTILS_OPENCV_BUILD_LIST}"
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
    # 由上面的模块清单派生，保证两个分支的模块集合不会漂移
    set(OpenCV_LIBS "")
    set(OpenCV_INCLUDE_DIRS "")
    foreach(_mod IN LISTS CAMERA_UTILS_OPENCV_MODULES)
        list(APPEND OpenCV_LIBS "opencv_${_mod}")
        list(APPEND OpenCV_INCLUDE_DIRS "${opencv_SOURCE_DIR}/modules/${_mod}/include")
    endforeach()
    foreach(_mod IN LISTS CAMERA_UTILS_OPENCV_CONTRIB_MODULES)
        list(APPEND OpenCV_LIBS "opencv_${_mod}")
        list(APPEND OpenCV_INCLUDE_DIRS "${opencv_contrib_SOURCE_DIR}/modules/${_mod}/include")
    endforeach()
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

# ============================================================
# exiv2（图像元数据：EXIF / IPTC / XMP）
# - 本地已有安装 → 快速加载
# - 否则从源码构建（静态库）
# ============================================================
if(DEFINED ENV{EXIV2_ROOT} AND EXISTS "$ENV{EXIV2_ROOT}")
    file(TO_CMAKE_PATH "$ENV{EXIV2_ROOT}" _exiv2_candidate)
    message(STATUS "exiv2: 候选路径来自环境变量 EXIV2_ROOT = ${_exiv2_candidate}")
elseif(EXISTS "D:/ThirdPartyLib/exiv2")
    set(_exiv2_candidate "D:/ThirdPartyLib/exiv2")
    message(STATUS "exiv2: 候选路径为本地 = ${_exiv2_candidate}")
else()
    set(_exiv2_candidate "")
    message(STATUS "exiv2: 无本地候选路径，将从源码构建")
endif()

if(_exiv2_candidate AND EXISTS "${_exiv2_candidate}/include/exiv2")
    # ---------- 分支 A：本地已安装 ----------
    message(STATUS "exiv2: 使用本地安装: ${_exiv2_candidate}")
    set(EXIV2_INCLUDE_DIR "${_exiv2_candidate}/include")
    file(GLOB _exiv2_lib_candidates
        "${_exiv2_candidate}/lib/exiv2*.lib"
        "${_exiv2_candidate}/lib/exiv2*.a"
        "${_exiv2_candidate}/lib64/exiv2*.lib"
        "${_exiv2_candidate}/lib64/exiv2*.a"
    )
    if(_exiv2_lib_candidates)
        list(GET _exiv2_lib_candidates 0 _exiv2_lib_file)
        set(EXIV2_LIBS "${_exiv2_lib_file}")
    else()
        message(FATAL_ERROR "exiv2 library not found under ${_exiv2_candidate}/lib")
    endif()
else()
    # ---------- 分支 B：从源码构建（静态库） ----------
    message(STATUS "exiv2: 从源码构建（静态库）")
    CPMAddPackage(
        NAME exiv2
        GITHUB_REPOSITORY Exiv2/exiv2
        GIT_TAG v0.28.7
        GIT_SHALLOW TRUE
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "EXIV2_BUILD_EXIV2 OFF"
            "EXIV2_ENABLE_PNG OFF" # ← 改为 OFF，消除 ZLIB 依赖
            "EXIV2_ENABLE_JPEG ON"
            "EXIV2_ENABLE_TIFF ON"
            "EXIV2_ENABLE_XMP OFF"      # ← 改为 OFF，消除 EXPAT 依赖
            "EXIV2_ENABLE_WEBP OFF"
            "EXIV2_ENABLE_INOTIFY OFF"
            "EXIV2_ENABLE_TESTING OFF"
            "EXIV2_ENABLE_BROTLI OFF"   # 新增：关闭 Brotli/JPEG XL 支持
            "EXIV2_ENABLE_INIH OFF"       # ← 新增：关闭 inih 依赖
    )
    set(EXIV2_INCLUDE_DIR "${exiv2_SOURCE_DIR}/include")
    set(EXIV2_LIBS exiv2lib)
endif()

message(STATUS "EXIV2_INCLUDE_DIR = ${EXIV2_INCLUDE_DIR}")
message(STATUS "EXIV2_LIBS = ${EXIV2_LIBS}")

# ============================================================
# Optional feature: VAS concurrency demo (requires stdexec)
# Enabled by default; disable with -DCAMERA_UTILS_ENABLE_VAS_CONCURRENCY=OFF
# ============================================================
option(CAMERA_UTILS_ENABLE_VAS_CONCURRENCY
    "Build the stdexec based VAS concurrency demo" ON)
message(STATUS "CAMERA_UTILS_ENABLE_VAS_CONCURRENCY = ${CAMERA_UTILS_ENABLE_VAS_CONCURRENCY}")

# ============================================================
# camera_utils_build_options: shared warning level / optimization options
# Used by camera_utils_core, CameraUtils and camera_utils_tests
# ============================================================
add_library(camera_utils_build_options INTERFACE)
add_library(CameraUtils::build_options ALIAS camera_utils_build_options)

target_compile_options(camera_utils_build_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Release>>:-O3>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2>
)

# Feature switch consumed by VasConcurrencyDemo.h
target_compile_definitions(camera_utils_build_options INTERFACE
    VAS_ENABLE_STDEXEC=$<BOOL:${CAMERA_UTILS_ENABLE_VAS_CONCURRENCY}>
)

enable_testing()
add_subdirectory(src)
add_subdirectory(tests)

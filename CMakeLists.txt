cmake_minimum_required (VERSION 3.15)

# 如果支持，请为 MSVC 编译器启用热重载
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

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
    )
endif()
set(CPM_SOURCE_CACHE "$ENV{USERPROFILE}/.cpm-cache" CACHE PATH "CPM source cache")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

set(FETCHCONTENT_SOURCE_DIR_RAPIDS-CMAKE "D:/ThirdPartyLib/rapids-cmake")

# nlohmann_json
CPMAddPackage(
    NAME json
    URL      https://hf-mirror.com/csukuangfj/sherpa-ncnn-cmake-deps/resolve/main/json-3.12.0.tar.gz
    URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OPTIONS
        "JSON_BuildTests OFF"
)

# stdexec
# 注意：stdexec 的 CMake 构建依赖 rapids-cmake，提前指定本地路径避免下载
set(FETCHCONTENT_SOURCE_DIR_RAPIDS-CMAKE "D:/ThirdPartyLib/rapids-cmake")

CPMAddPackage(
    NAME stdexec
    GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    OPTIONS
        "STDEXEC_BUILD_EXAMPLES OFF"
        "STDEXEC_BUILD_TESTS OFF"
)

# opencv
CPMAddPackage(
    NAME opencv_prebuilt
    SOURCE_DIR "D:/ThirdPartyLib/opencv/opencv_windows/install"
    DOWNLOAD_ONLY YES
)
set(_opencv_candidates
    "${opencv_prebuilt_SOURCE_DIR}/build"
    "${opencv_prebuilt_SOURCE_DIR}/build/install"
    "${opencv_prebuilt_SOURCE_DIR}"
)
set(OpenCV_DIR "" CACHE PATH "Path to OpenCVConfig.cmake" FORCE)

foreach(_cand IN LISTS _opencv_candidates)
    if(EXISTS "${_cand}")
        file(GLOB_RECURSE _config_files "${_cand}/OpenCVConfig.cmake")
        if(_config_files)
            list(GET _config_files 0 _first_config)
            get_filename_component(OpenCV_DIR "${_first_config}" DIRECTORY)
            break()
        endif()
    endif()
endforeach()

if(NOT OpenCV_DIR OR NOT EXISTS "${OpenCV_DIR}/OpenCVConfig.cmake")
    message(FATAL_ERROR
        "OpenCVConfig.cmake not found under ${opencv_prebuilt_SOURCE_DIR}.\n"
        "请检查预编译包解压后的实际目录结构，或改用包含 CMake 配置的 zip 包。")
endif()

message(STATUS "OpenCV_DIR = ${OpenCV_DIR}")
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

# ffmpeg
set(FFMPEG_PREBUILT_URL
    "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip"
)
CPMAddPackage(
    NAME ffmpeg_prebuilt
    URL  "${FFMPEG_PREBUILT_URL}"
    DOWNLOAD_ONLY YES          # 关键：只下载解压，不执行 add_subdirectory
    # URL_HASH SHA256=xxxxxxxx # 可选，跑通后再补
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

set(FFMPEG_LIBS
    avcodec
    avformat
    avutil
    swscale
    swresample
    avfilter
    avdevice
)

if(NOT EXISTS "${FFMPEG_INCLUDE_DIR}/libavcodec/avcodec.h")
    message(FATAL_ERROR
        "FFmpeg include not found.\n"
        "  FFMPEG_ROOT        = ${FFMPEG_ROOT}\n"
        "  FFMPEG_INCLUDE_DIR = ${FFMPEG_INCLUDE_DIR}\n"
        "  ffmpeg_prebuilt_SOURCE_DIR = ${ffmpeg_prebuilt_SOURCE_DIR}\n"
        "请检查 _deps/ffmpeg_prebuilt-src 或缓存目录下的实际解压结构。")
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
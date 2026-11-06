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
# OpenCV（本地/CI 双路径）
# ============================================================
camera_utils_resolve_lib_root(OpenCV OPENCV_ROOT
    "D:/ThirdPartyLib/opencv/opencv_windows/install")

CPMAddPackage(
    NAME opencv_prebuilt
    SOURCE_DIR "${OPENCV_ROOT}"
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
        "OpenCVConfig.cmake not found under ${opencv_prebuilt_SOURCE_DIR}.")
endif()

message(STATUS "OpenCV_DIR = ${OpenCV_DIR}")
find_package(OpenCV REQUIRED COMPONENTS
    core imgproc highgui features2d calib3d flann video imgcodecs objdetect)

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
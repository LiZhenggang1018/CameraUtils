# 生成可执行程序 CameraUtils
add_executable(CameraUtils main.cpp)

target_sources(CameraUtils PRIVATE
    LivePhotoSmoothPlay/ImageRegistrator.cpp
    LivePhotoSmoothPlay/YUVImageReader.cpp
    OpticalFlow/LkTracker.cpp
    OpticalFlow/MiddleburyColor.cpp
    OpticalFlow/Visualization.cpp
    OpticalFlow/BiDirectionalLkFilter.cpp
)

target_include_directories(CameraUtils PRIVATE
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/inc/LivePhotoSmoothPlay
    ${CMAKE_SOURCE_DIR}/inc/ImageReg
    ${CMAKE_SOURCE_DIR}/inc/OpticalFlow
    ${CMAKE_SOURCE_DIR}/tests
)
target_include_directories(CameraUtils SYSTEM PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIR}
)

target_link_directories(CameraUtils PRIVATE
    ${FFMPEG_LIB_DIR}
)

target_link_libraries(CameraUtils PRIVATE
    ${OpenCV_LIBS}
    opencv_optflow
    ${FFMPEG_LIBS}
    STDEXEC::stdexec
    nlohmann_json::nlohmann_json
    magic_enum::magic_enum
)

target_compile_options(CameraUtils PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Release>>:-O3>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2>
)

set_target_properties(CameraUtils PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_compile_definitions(CameraUtils PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)

# ---------- DLL 复制（本地/CI 通用） ----------
# FFmpeg
camera_utils_copy_dlls(CameraUtils "${FFMPEG_ROOT}/bin")

# OpenCV
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_opencv_dll_suffix "d")
else()
    set(_opencv_dll_suffix "")
endif()

file(GLOB_RECURSE _opencv_dlls CONFIGURE_DEPENDS
    "${opencv_prebuilt_SOURCE_DIR}/opencv_world*${_opencv_dll_suffix}.dll"
    "${opencv_prebuilt_SOURCE_DIR}/opencv_core*${_opencv_dll_suffix}.dll"
)
if(_opencv_dlls)
    list(GET _opencv_dlls 0 _first_dll)
    get_filename_component(_opencv_bin_dir "${_first_dll}" DIRECTORY)
    camera_utils_copy_dlls(CameraUtils "${_opencv_bin_dir}")
endif()
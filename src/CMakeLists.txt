# ============================================================
# camera_utils_core：可复用的静态库（算法与工具实现）
# ============================================================
file(GLOB LIVE_PHOTO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/LivePhotoSmoothPlay/*.cpp"
)
file(GLOB OPTICAL_FLOW_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/OpticalFlow/*.cpp"
)
file(GLOB IMAGE_PROCESSING_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/ImageProcessing/*.cpp"
)
file(GLOB FEATURE_MATCHING_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/FeatureMatching/*.cpp"
)
file(GLOB ALIGNMENT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Alignment/*.cpp"
)

add_library(camera_utils_core STATIC
    ${LIVE_PHOTO_SOURCES}
    ${OPTICAL_FLOW_SOURCES}
    ${IMAGE_PROCESSING_SOURCES}
    ${FEATURE_MATCHING_SOURCES}
    ${ALIGNMENT_SOURCES}
)
add_library(CameraUtils::core ALIAS camera_utils_core)

target_include_directories(camera_utils_core PUBLIC
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/inc/CommonUtils
    ${CMAKE_SOURCE_DIR}/inc/LivePhotoSmoothPlay
    ${CMAKE_SOURCE_DIR}/inc/OpticalFlow
    ${CMAKE_SOURCE_DIR}/inc/ComputationalPhotography
    ${CMAKE_SOURCE_DIR}/inc/ImageProcessing
    ${CMAKE_SOURCE_DIR}/inc/FeatureMatching
    ${CMAKE_SOURCE_DIR}/inc/Alignment
)
target_include_directories(camera_utils_core SYSTEM PUBLIC
    ${OpenCV_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIR}
    ${EXIV2_INCLUDE_DIR}
    ${CMAKE_BINARY_DIR}        # exiv2lib_export.h 所在目录
)

target_link_directories(camera_utils_core PUBLIC
    ${FFMPEG_LIB_DIR}
)

target_link_libraries(camera_utils_core PUBLIC
    ${OpenCV_LIBS}
    ${FFMPEG_LIBS}
    ${EXIV2_LIBS}
    nlohmann_json::nlohmann_json
    magic_enum::magic_enum
)
target_link_libraries(camera_utils_core PRIVATE
    STDEXEC::stdexec
    CameraUtils::build_options
)

set_target_properties(camera_utils_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 头文件依赖 ProjectRootDir，必须对使用方可见
target_compile_definitions(camera_utils_core PUBLIC
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)

# ============================================================
# CameraUtils：命令行驱动层（main + 命令表）
# ============================================================
add_executable(CameraUtils main.cpp)

target_include_directories(CameraUtils PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/tests/CommonUtils
)

target_link_libraries(CameraUtils PRIVATE
    CameraUtils::core
    CameraUtils::build_options
)

# VasConcurrencyDemo.h is reached via tests/cli/CommandTable.h
if(CAMERA_UTILS_ENABLE_VAS_CONCURRENCY)
    target_link_libraries(CameraUtils PRIVATE STDEXEC::stdexec)
endif()

set_target_properties(CameraUtils PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# DLL 运行库复制（FFmpeg + 当前构建类型的 OpenCV）
camera_utils_copy_runtime_dlls(CameraUtils)

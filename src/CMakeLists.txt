# 生成可执行程序 CameraUtils
add_executable(CameraUtils main.cpp)

# 自动扫描两个模块目录下的 .cpp（不递归子目录）
file(GLOB LIVE_PHOTO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/LivePhotoSmoothPlay/*.cpp"
)
file(GLOB OPTICAL_FLOW_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/OpticalFlow/*.cpp"
)

target_sources(CameraUtils PRIVATE
    ${LIVE_PHOTO_SOURCES}
    ${OPTICAL_FLOW_SOURCES}
)

target_include_directories(CameraUtils PRIVATE
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/inc/LivePhotoSmoothPlay
    ${CMAKE_SOURCE_DIR}/inc/ImageReg
    ${CMAKE_SOURCE_DIR}/inc/OpticalFlow
    ${CMAKE_SOURCE_DIR}/inc/HdrMerge
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
# FFmpeg：BtbN 包只有一套 DLL，直接整目录复制
camera_utils_copy_dlls(CameraUtils "${FFMPEG_ROOT}/bin")

# OpenCV：精确复制当前构建类型对应的 DLL，避免 Debug/Release 混用
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug：只要以 d.dll 结尾的（opencv_world4100d.dll / opencv_core4100d.dll ...）
    file(GLOB_RECURSE _opencv_dlls CONFIGURE_DEPENDS
        "${opencv_prebuilt_SOURCE_DIR}/opencv_world*d.dll"
        "${opencv_prebuilt_SOURCE_DIR}/opencv_core*d.dll"
    )
else()
    # Release：先 glob 全部，再用 FILTER 精确排除以 d.dll 结尾的
    file(GLOB_RECURSE _opencv_dlls CONFIGURE_DEPENDS
        "${opencv_prebuilt_SOURCE_DIR}/opencv_world*.dll"
        "${opencv_prebuilt_SOURCE_DIR}/opencv_core*.dll"
    )
    # 关键：排除 opencv_world4100d.dll 这类 Debug 版
    list(FILTER _opencv_dlls EXCLUDE REGEX "d\\.dll$")
endif()

if(_opencv_dlls)
    foreach(_dll IN LISTS _opencv_dlls)
        message(STATUS "Will copy OpenCV DLL: ${_dll}")
        add_custom_command(TARGET CameraUtils POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_dll}"
                "$<TARGET_FILE_DIR:CameraUtils>"
            COMMENT "Copying OpenCV DLL: ${_dll}"
        )
    endforeach()
else()
    message(WARNING "No OpenCV DLL found to copy under ${opencv_prebuilt_SOURCE_DIR}")
endif()
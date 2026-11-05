# 生成可执行程序 CameraUtils
add_executable(CameraUtils main.cpp)

target_sources(CameraUtils PRIVATE
    LivePhotoSmoothPlay/ImageRegistrator.cpp
    LivePhotoSmoothPlay/YUVImageReader.cpp
)

target_include_directories(CameraUtils PRIVATE
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/inc/LivePhotoSmoothPlay
    ${CMAKE_SOURCE_DIR}/tests
    ${OpenCV_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIR}
)

target_link_directories(CameraUtils PRIVATE
    ${FFMPEG_LIB_DIR}              # ← FFmpeg 导入库所在目录
)

target_link_libraries(CameraUtils PRIVATE
    ${OpenCV_LIBS}
    ${FFMPEG_LIBS}
    STDEXEC::stdexec  # 添加这一行
    nlohmann_json::nlohmann_json
)

# 编译选项：区分编译器和构建类型
target_compile_options(CameraUtils PRIVATE
    # GCC / Clang 通用警告
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>

    # MSVC 通用警告
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>        # <-- 新增这一行

    # 强制 Release 下用 -O3（若想覆盖默认的 -O3，其实默认已是 -O3）
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Release>>:-O3>

    # MSVC Release 用 /O2
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2>
)

# C++ 标准
set_target_properties(CameraUtils PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_compile_definitions(CameraUtils PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)

# 构建后把 FFmpeg 的 DLL 复制到 CameraUtils.exe 所在目录,否则运行时报错：找不到avcodec-63.dll
add_custom_command(TARGET CameraUtils POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${FFMPEG_ROOT}/bin"
        "$<TARGET_FILE_DIR:CameraUtils>"
    COMMENT "Copying FFmpeg DLLs to $<TARGET_FILE_DIR:CameraUtils>"
)
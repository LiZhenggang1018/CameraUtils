# 生成可执行程序 CameraUtils
add_executable(CameraUtils main.cpp)

target_include_directories(CameraUtils PRIVATE
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json
)

# 编译选项：区分编译器和构建类型
target_compile_options(CameraUtils PRIVATE
    # GCC / Clang 通用警告
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>

    # MSVC 通用警告
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->

    # 强制 Release 下用 -O3（若想覆盖默认的 -O3，其实默认已是 -O3）
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Release>>:-O3>

    # MSVC Release 用 /O2
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2>
)

# C++ 标准
set_target_properties(CameraUtils PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
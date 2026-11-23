# ============================================================
# camera_utils_tests：独立测试可执行文件（CTest 驱动）
# ============================================================
add_executable(camera_utils_tests TestMain.cpp)

target_include_directories(camera_utils_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/tests/CommonUtils
)

target_link_libraries(camera_utils_tests PRIVATE
    CameraUtils::core
    CameraUtils::build_options
)

# Link stdexec only when the VAS demo is enabled
if(CAMERA_UTILS_ENABLE_VAS_CONCURRENCY)
    target_link_libraries(camera_utils_tests PRIVATE STDEXEC::stdexec)
endif()

set_target_properties(camera_utils_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 测试可执行同样需要 FFmpeg/OpenCV 运行库
camera_utils_copy_runtime_dlls(camera_utils_tests)

# ------------------------------------------------------------
# CTest 注册：逐个用例注册，按 label 分组
# ------------------------------------------------------------
set(_unit_tests
    deque-blocking
    deque-nonblocking
    find-then-or-else
    overload-ref
    timeout-monitor
    misc
    circular-queue
    overwrite-function
    msg-queue
    event
    factory-json
    sync-agency
    thread-pool
    instance-counter
    type2instmap
    magic-enum
    sobel
    linear-filter
    nonlinear-filter
    pyramid
    keypoint-detect
    local-feature
    feature-match
    homography
    blending
    exposure-comp
    vignetting
    panorama
    lk-tracker
    dense-flow
    frame-interp
    eis
    hdr-merge-unit
    mfnr
    exif
)

# The vas-concurrency case only exists when the option is enabled
if(CAMERA_UTILS_ENABLE_VAS_CONCURRENCY)
    list(APPEND _unit_tests vas-concurrency)
endif()

foreach(_t IN LISTS _unit_tests)
    add_test(NAME "unit.${_t}" COMMAND camera_utils_tests ${_t})
    set_tests_properties("unit.${_t}" PROPERTIES
        LABELS "unit"
        SKIP_RETURN_CODE 77
        TIMEOUT 120
    )
endforeach()

# media 组：仅在 data/ 目录存在时注册（CI 通常无素材）
if(EXISTS "${CMAKE_SOURCE_DIR}/data")
    set(_media_tests
        ffmpeg
        optical-flow-upsample
        hdr-merge
        hdr-merge-multilevel
        hdr-merge-physical
        hdr-merge-ghost
        livephoto
        livephoto-reg
    )
    foreach(_t IN LISTS _media_tests)
        add_test(NAME "media.${_t}" COMMAND camera_utils_tests ${_t})
        set_tests_properties("media.${_t}" PROPERTIES
            LABELS "media"
            SKIP_RETURN_CODE 77
            TIMEOUT 900
        )
    endforeach()
else()
    message(STATUS "data/ 目录不存在，跳过 media 类测试注册")
endif()

# gui 组需要图形界面，默认不注册（可通过 CameraUtils <command> 手动运行）

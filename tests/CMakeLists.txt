# 注册一个 CTest 测试。
# 运行 ctest 时，会执行 CameraUtils，而 CameraUtils 会调用 TestCameraUtils()。
add_test(NAME TestCameraUtils COMMAND CameraUtils)
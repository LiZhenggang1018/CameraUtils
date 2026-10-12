项目名称：CameraUtils

项目简介：例如“Utils code for camera HAL use”

构建与测试命令：

bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
依赖说明（如 nlohmann_json 的引入方式）
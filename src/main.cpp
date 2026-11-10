#include "CommonHead.h"
#include "RunTestCase.h"

using namespace std;

namespace {
struct StartupProbe {
    StartupProbe() { std::fprintf(stderr, "[STARTUP] global init running\n"); std::fflush(stderr); }
};
static StartupProbe g_startup_probe;
}
int main()
{
    std::fprintf(stderr, "[MAIN] enter main\n");
    std::fflush(stderr);
    SetConsoleOutputCP(65001); // 控制台输出切换为 UTF-8
    RunTest();
    int ch = getchar();
    return 0;
}

#include "CommonHead.h"
#include "TestCase.h"

using namespace std;

int main()
{
    SetConsoleOutputCP(65001); // 控制台输出切换为 UTF-8
    RunAllTests();
    int ch = getchar();
    return 0;
}

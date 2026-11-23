#ifndef TESTS_CPPRUNTIMECHECK_CPPRUNTIMECHECKDEMO_H
#define TESTS_CPPRUNTIMECHECK_CPPRUNTIMECHECKDEMO_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

// CppRuntimeCheck —— 运行时检查工具验证 demo。
//
// 每个函数「故意触发」一类运行时错误，用来验证对应的检查工具是否生效：
//   - ASan 堆越界       → AsanHeapOverflow()
//   - ASan use-after-free → AsanUseAfterFree()
//   - ASan 栈越界       → AsanStackOverflow()
//   - _CRTDBG 内存泄漏  → CrtDbgLeak()
//   - UBSan 未定义行为  → UbsanUndefinedBehavior()
//   - /GS 栈 cookie 溢出 → GsStackCookieOverflow()
//   - CFG 间接调用演示  → CfiIndirectCall()（防御性，不报错）
//
// ⚠️ 这些函数会故意崩溃/触发检查，绝不能注册进 ctest（CI 会挂），仅供手动运行验证。

namespace CppRuntimeCheck {

// ASan 堆缓冲区越界写：new 出 16 字节，写第 17 个元素。
inline void AsanHeapOverflow()
{
    int* buf = new int[4]; // 16 字节
    std::cout << "[AsanHeapOverflow] writing out of bounds..." << std::endl;
    buf[4] = 42; // 越界写（应为 buf[3]）
    std::cout << "buf[4] = " << buf[4] << std::endl;
    delete[] buf;
}

// ASan use-after-free：free 后再次读写。
inline void AsanUseAfterFree()
{
    int* p = new int(7);
    delete p;
    std::cout << "[AsanUseAfterFree] reading freed memory..." << std::endl;
    std::cout << "*p = " << *p << std::endl; // use-after-free
}

// ASan 栈缓冲区越界写：栈上数组越界。
inline void AsanStackOverflow()
{
    int arr[4] = {0, 1, 2, 3};
    std::cout << "[AsanStackOverflow] writing out of bounds on stack..." << std::endl;
    arr[4] = 99; // 栈越界写
    std::cout << "arr[4] = " << arr[4] << std::endl;
}

// _CRTDBG 内存泄漏：new 了不 delete，程序退出时 _CRTDBG 报告泄漏。
inline void CrtDbgLeak()
{
    std::cout << "[CrtDbgLeak] leaking 100 bytes..." << std::endl;
    char* leak = new char[100];
    (void)leak; // 故意不释放
    std::cout << "[CrtDbgLeak] 函数返回，泄漏的 100 字节将在进程退出时被 _CRTDBG 报告" << std::endl;
}

// UBSan 未定义行为：有符号整数溢出。
// 注意：MSVC 不支持 UBSan，需 clang-cl 编译 + -fsanitize=undefined 才会报告。
inline void UbsanUndefinedBehavior()
{
    std::cout << "[UbsanUndefinedBehavior] signed integer overflow..." << std::endl;
    int x = 2147483647; // INT_MAX
    int y = x + 1;      // 有符号溢出 = UB
    std::cout << "INT_MAX + 1 = " << y << std::endl;
}

// /GS 栈 cookie 溢出：栈上数组越界写，覆盖返回地址前的「金丝雀」值，
// 函数返回时 /GS 检测到 cookie 被破坏，触发安全检查失败（异常终止）。
// 需要 /GS 开启（MSVC Debug/Release 默认开启）。
inline void GsStackCookieOverflow()
{
    std::cout << "[GsStackCookieOverflow] overflowing stack buffer to corrupt cookie..." << std::endl;
    // 用 volatile 指针间接写，绕开 /sdl 的编译期静态边界分析（否则 hardened 构建报 C4789），
    // 但运行时仍会越界覆盖 /GS 插入的栈 cookie，函数返回时 /GS 报错。
    volatile char buf[16];
    volatile char* p = buf;
    for (int i = 0; i < 64; ++i) {
        p[i] = 'A';
    }
    std::cout << "buf = " << buf << std::endl; // 函数返回时 /GS 报错
}

// CFG 间接调用演示：通过函数指针做间接调用，展示「CFG 保护的调用点」。
// 注意：CFG（/guard:cf）是防御性机制，正常代码路径下不会报错——它只在
// 函数指针被攻击者篡改（指向非法地址）时才拦截。此 demo 仅演示合法的间接调用。
inline void CfiIndirectCall()
{
    using FnPtr = void (*)();
    FnPtr fp = []() {
        std::cout << "[CfiIndirectCall] indirect call executed (CFG 保护中，正常调用不报错)" << std::endl;
    };
    std::cout << "[CfiIndirectCall] invoking function pointer..." << std::endl;
    fp();
}

} // namespace CppRuntimeCheck

#endif // TESTS_CPPRUNTIMECHECK_CPPRUNTIMECHECKDEMO_H

#ifndef TESTS_CLI_TESTASSERT_H
#define TESTS_CLI_TESTASSERT_H

#include <iostream>
#include <string>

namespace cu
{

/// 全局失败计数：由 TestAssert 的检查宏累加，测试 runner 据此返回退出码。
inline int& FailureCount()
{
    static int count = 0;
    return count;
}

inline void ResetFailures()
{
    FailureCount() = 0;
}

/// 检查失败时打印位置并累加计数（Release 下同样生效，不依赖 assert/NDEBUG）
inline bool Check(bool condition, const char* expr, const char* file, int line)
{
    if (!condition)
    {
        ++FailureCount();
        std::cerr << "[FAIL] " << file << ":" << line << "  " << expr << std::endl;
        return false;
    }
    return true;
}

} // namespace cu

#define CU_CHECK(expr) ::cu::Check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

#define CU_CHECK_EQ(actual, expected) \
    ::cu::Check((actual) == (expected), #actual " == " #expected, __FILE__, __LINE__)

#endif // TESTS_CLI_TESTASSERT_H

/*
 * @Description: print variable agrs with space between
 */

#ifndef CUSTOM_LOG_H
#define CUSTOM_LOG_H

#include <iostream>

// 通用打印函数
template <typename... Args>
void PV(Args&&... args)
{
    ((std::cout << args << ' '), ...);
    std::cout << std::endl;
}

// template<typename T>
// void PrintArg(T&& arg) {
//     using ArgType = std::decay_t<T>;
//     if constexpr (std::is_pointer_v<ArgType>) {
//         std::cout << (reinterpret_cast<uintptr_t>(arg) & 0xFFFFFF); // 仅保留16进制的低6位
//     }
//     else {
//         std::cout << arg;
//     }
// }
//
// template<typename... Args>
// void PV(Args&&... args) {
//     (PrintArg(std::forward<Args>(args)), ...);
//     std::cout << std::endl;
// }

#endif // CUSTOM_LOG_H

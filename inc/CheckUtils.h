/*
 * @Description: check utils header
 */

#ifndef CHECKUTILS_H
#define CHECKUTILS_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "CustomLog.h"

#ifndef CHECK_AND_RETURN
#define CHECK_AND_RETURN(cond)   \
    do                           \
    {                            \
        if (cond)                \
        {                        \
            PV(#cond, "failed"); \
            return;              \
        }                        \
    } while (0)
#endif

#ifndef CHECK_AND_RETURN_RET
#define CHECK_AND_RETURN_RET(cond, retval) \
    do                                     \
    {                                      \
        if (cond)                          \
        {                                  \
            PV(#cond, "failed");           \
            return retval;                 \
        }                                  \
    } while (0)
#endif

#ifndef CAR
#define CAR(cond)                \
    do                           \
    {                            \
        if (cond)                \
        {                        \
            PV(#cond, "failed"); \
            return;              \
        }                        \
    } while (0)
#endif

#ifndef CARR
#define CARR(cond, retval)       \
    do                           \
    {                            \
        if (cond)                \
        {                        \
            PV(#cond, "failed"); \
            return retval;       \
        }                        \
    } while (0)
#endif

// 判断2个任意的相同浮点类型是否相等
template <typename T>
bool FEqual(T x, T y, T tolerance = std::numeric_limits<T>::epsilon())
{
    static_assert(std::is_floating_point_v<T>, "Template param must be floating point");
    return std::fabs(x - y) <= tolerance;
}

// 大小端转换
// impl endian conversion / byte order conversion, from Big-endian to Little-endian or vice versa;
template <typename T>
T SwapEndian(T value)
{
    static_assert(std::is_arithmetic<T>::value, "Template param must be an arithmetic type");
    T result{};
    uint8_t* src = reinterpret_cast<uint8_t*>(&value);
    uint8_t* dst = reinterpret_cast<uint8_t*>(&result);
    std::reverse_copy(src, src + sizeof(T), dst);
    return result;
}

// 原位大小端转换
// impl InPlace endian conversion;
template <typename T>
void SwapEndianInPlace(T& value)
{
    value = SwapEndian(value);
}

// 函数用于判断字符串是否以指定的子串结尾
inline bool EndsWith(const std::string& str, const std::string& suffix)
{
    CHECK_AND_RETURN_RET(suffix.size() > str.size(), false);
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

// 函数用于判断字符串是否以指定的子串开头
inline bool StartsWith(const std::string& str, const std::string& prefix)
{
    CHECK_AND_RETURN_RET(prefix.size() > str.size(), false);
    return str.compare(0, prefix.size(), prefix) == 0;
}

// 辅助函数，用于递归展开元组并调用函数
template <typename Func, typename Tuple, std::size_t... Index>
void TupleForEachImpl(Func&& func, Tuple&& tuple, std::index_sequence<Index...>)
{
    (func(std::get<Index>(tuple)), ...);
}

// 主函数，用于遍历元组并对每个元素调用给定的函数
template <typename Func, typename... Ts>
void TupleForEach(Func&& func, std::tuple<Ts...>& tuple)
{
    TupleForEachImpl(std::forward<Func>(func), tuple, std::index_sequence_for<Ts...>{});
}

#endif // CHECKUTILS_H

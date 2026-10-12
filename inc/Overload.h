/*
 * @Description: 2 kinds of Overload impl (support and not support implicit type conversion)
 */

#ifndef OVERLOAD_H
#define OVERLOAD_H

// Overload support implicit type conversion,
// meanwhile visitor can add impl like auto/aut&& lambda
template <typename... T>
struct Overload : T...
{
    using T::operator()...;
};

template <typename... T>
Overload(T...) -> Overload<T...>;

// OverloadNoCvt do not support implicit type conversion,
// meanwhile visitor should not add impl like auto/aut&& lambda (useless)
template <typename...>
constexpr bool ALWAYS_FALSE_VAL{false};

template <typename... Ts>
struct OverloadNoCvt : Ts...
{
    using Ts::operator()...;

    // Prevent implicit type conversions
    template <typename T>
    constexpr void operator()(T) const
    {
        static_assert(ALWAYS_FALSE_VAL<T>, "Unsupported type");
    }
};

template <typename... T>
OverloadNoCvt(T...) -> OverloadNoCvt<T...>;

#endif // OVERLOAD_H

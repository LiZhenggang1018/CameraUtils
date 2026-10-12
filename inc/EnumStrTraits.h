#ifndef ENUM_STR_TRAITS_H
#define ENUM_STR_TRAITS_H

#if !defined(__PRETTY_FUNCTION__) && !defined(__GNUC__)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#include <optional>
#include <string>
#include <string_view>

/* DO NOT INCLUDE THIS FILE INSIDE NAMESAPCE */
enum ProbeEnum
{
    PROBE_ENUM
}; // for probe usage
namespace EnumTraits
{

inline constexpr size_t maxValidEnums = 128;

template <typename T>
constexpr const char* PrettyTypeName()
{
    return __PRETTY_FUNCTION__;
}

template <auto Ev>
constexpr const char* PrettyEnumName()
{
    return __PRETTY_FUNCTION__;
}

constexpr std::string_view
AnchorCounterpart(std::string_view probe, std::string_view sig, std::string_view anchor)
{
    auto prefix = probe.find(anchor);
    auto suffix = probe.length() - prefix - anchor.length();
    if (prefix != std::string_view::npos)
    {
        sig.remove_prefix(prefix);
        sig.remove_suffix(suffix);
    }
    return sig;
}

template <auto Ev>
constexpr std::string_view ExtractEnumName()
{
    return AnchorCounterpart(PrettyEnumName<PROBE_ENUM>(), PrettyEnumName<Ev>(), "PROBE_ENUM");
}

template <typename T>
constexpr std::string_view ExtractTypeName()
{
    return AnchorCounterpart(PrettyTypeName<ProbeEnum>(), PrettyTypeName<T>(), "ProbeEnum");
}

constexpr std::string_view UnscopeStr(std::string_view sig)
{
    auto pos = sig.rfind(':');
    if (pos != std::string_view::npos)
    {
        return sig.substr(pos + 1);
    }
    return sig;
}

constexpr std::string_view RemovePossibleParenthesis(std::string_view sig)
{
    if (sig.empty() || sig[0] != '(')
    {
        return sig;
    }
    auto right = sig.find(')');
    if (right != std::string_view::npos)
    {
        return sig.substr(right + 1);
    }
    return sig;
}

template <typename Enum>
constexpr std::string_view ExtractEnumTypeName()
{
    constexpr auto name = ExtractTypeName<Enum>();
    static_assert(!name.empty(), "failed to extract enum type name");
    return UnscopeStr(name);
}

template <auto Ev, typename = std::enable_if_t<std::is_enum_v<decltype(Ev)>>>
constexpr bool IsValidEnumValue()
{
    constexpr auto name = ExtractEnumName<Ev>();
    constexpr auto integerName = ExtractEnumName<std::underlying_type_t<decltype(Ev)>(Ev)>();
    static_assert(!name.empty(), "failed to extract enum value name");
    return RemovePossibleParenthesis(name).compare(integerName) != 0;
}

template <auto Ev>
constexpr std::string_view ExtractUnScopedEnumName()
{
    constexpr auto name = ExtractEnumName<Ev>();
    static_assert(!name.empty(), "failed to extract enum value name");
    return UnscopeStr(name);
}

template <typename Enum>
using ULT = std::underlying_type_t<Enum>;

template <typename Enum>
struct EnumRange
{
    using type = ULT<Enum>;
    static constexpr type begin = std::is_signed_v<type> ? -16 : 0; // inclusive
    static constexpr type end = std::is_signed_v<type> ? 16 : 32;   // exclusive
};

template <typename Enum>
constexpr auto ERBegin = EnumRange<Enum>::begin;
template <typename Enum>
constexpr auto EREnd = EnumRange<Enum>::end;
template <typename Enum>
constexpr size_t ERSize = EREnd<Enum> - ERBegin<Enum>;

constexpr size_t SVHash(std::string_view str)
{
    size_t hash{};
    for (auto c : str)
    {
        hash ^= static_cast<size_t>(c);
        hash <<= 1U;
    }
    return hash;
}

template <typename Enum,
          ULT<Enum> Ev = ERBegin<Enum>,
          bool = (Ev == EREnd<Enum>),
          bool = IsValidEnumValue<Enum(Ev)>()>
struct EnumStrInfo;

template <typename Enum, ULT<Enum> Ev>
struct EnumStrInfo<Enum, Ev, false, true>
{
    static constexpr size_t validEnums = EnumStrInfo<Enum, Ev + 1>::validEnums;
    static constexpr std::string_view svstr = ExtractUnScopedEnumName<Enum(Ev)>();
    static constexpr size_t svhash = SVHash(svstr);

    static constexpr std::string_view Value2Str(ULT<Enum> val)
    {
        return val == Ev ? svstr : EnumStrInfo<Enum, Ev + 1>::Value2Str(val);
    }

    static constexpr std::optional<Enum> Str2Enum(std::string_view str, size_t hash)
    {
        if (hash == svhash && svstr.compare(str) == 0)
        {
            return Enum(Ev);
        }
        return EnumStrInfo<Enum, Ev + 1>::Str2Enum(str, hash);
    }
};

template <typename Enum, ULT<Enum> Ev>
struct EnumStrInfo<Enum, Ev, false, false> : EnumStrInfo<Enum, Ev + 1>
{
};

template <typename Enum, ULT<Enum> Ev, bool valid>
struct EnumStrInfo<Enum, Ev, true, valid>
{
    static constexpr size_t validEnums = 0;
    static constexpr std::string_view Value2Str(ULT<Enum>)
    {
        return "InvalidEnumValue";
    }
    static constexpr std::optional<Enum> Str2Enum(std::string_view, size_t)
    {
        return std::nullopt;
    }
};

template <typename Enum, typename = std::enable_if_t<std::is_enum_v<Enum>>>
struct EnumCast
{
    static constexpr size_t validEnums = EnumStrInfo<Enum>::validEnums;
    static_assert(validEnums <= maxValidEnums, "support at max 128 enum values");

    static std::string ToStr(ULT<Enum> val)
    {
        return std::string(EnumStrInfo<Enum>::Value2Str(val));
    }
    static std::string ToStr(Enum enumVal)
    {
        return ToStr(static_cast<ULT<Enum>>(enumVal));
    }

    static std::string TypeStr()
    {
        return std::string(ExtractEnumTypeName<Enum>());
    }

    static std::optional<Enum> ToEnum(std::string_view evstr)
    {
        return EnumStrInfo<Enum>::Str2Enum(evstr, SVHash(evstr));
    }
};
} // namespace EnumTraits

template <typename Enum>
using EnumCast = EnumTraits::EnumCast<Enum>;

template <typename Enum>
std::string Enum2Str(EnumTraits::ULT<Enum> ev)
{
    return EnumTraits::EnumCast<Enum>::ToStr(ev);
}

template <typename Enum>
std::optional<Enum> Str2Enum(std::string_view str)
{
    return EnumTraits::EnumCast<Enum>::ToEnum(str);
}

// THIS MACRO CAN ONLY BE USED INSIDE HalCamera namespace
#define CUSTOMIZE_ENUM_RANGE(Enum, Beg, End)                     \
    namespace EnumTraits                                         \
    {                                                            \
    template <>                                                  \
    struct EnumRange<Enum>                                       \
    {                                                            \
        using type = ULT<Enum>;                                  \
        static constexpr type begin = static_cast<type>(Beg);    \
        static constexpr type end = static_cast<type>(End);      \
        static_assert(begin < end, #Enum " enum range invalid"); \
    };                                                           \
    }

#endif // ENUM_STR_TRAITS_H

#ifndef JSONUTILS_H
#define JSONUTILS_H

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "CheckUtils.h"
#include "CustomLog.h"
#include "EnumStrTraits.h"
#include "NoCopyableMacro.h"
#include "ScopedUtils.h"
#include "Singleton.h"

#if __has_include(<json.hpp>)
#include <json.hpp>
#define HAS_NLOHMANN_JSON_H 1
#else
#define HAS_NLOHMANN_JSON_H 0
#endif

#ifdef HAS_NLOHMANN_JSON_H

// using json = nlohmann::json;
namespace
{
constexpr int DEFAULT_INDENT{4}; // json 文件缩进空格数
const std::string CONFIG_PARAM_PATH{R"(configs\JsonConfig.json)"};
// 用于配置 json 序列化/反序列化参数的 json 文件路径，
// enableDump默认为0，表示禁用json dump功能（在log中打印序列化的json文件的每一项），配置为1表示启用
} // namespace

// add user-defined type's to_json/from json func, and import to nlohmann namespace
namespace nlohmann
{

// add to_json from_json support for std::optional
template <typename T>
struct adl_serializer<std::optional<T>>
{
    static void to_json(json& j, const std::optional<T>& opt)
    {
        if (!opt)
        {
            j = nullptr;
        }
        else
        {
            j = *opt; // this will call adl_serializer<T>::to_json which will
            // find the free function to_json in T's namespace!
        }
    }

    static void from_json(const json& j, std::optional<T>& opt)
    {
        if (j.is_null())
        {
            opt = {};
        }
        else
        {
            opt = j.template get<T>(); // same as above, but with
            // adl_serializer<T>::from_json
        }
    }
};

// 暂不支持std::variant多类型反序列化，因为其实现内部使用了异常处理机制
// [deprecated] involved with exception used by 3rd party lib
// add to_json from_json support for std::variant
template <typename T, typename... Ts>
void variant_from_json(const json& j, std::variant<Ts...>& data)
{
    if (j.is_null())
    {
        PV("josn id null");
    }
    else
    {
        data = j.template get<T>();
    }
}

template <typename... Ts>
struct adl_serializer<std::variant<Ts...>>
{
    static void to_json(json& j, const std::variant<Ts...>& data)
    {
        std::visit([&j](const auto& v) { j = v; }, data);
    }

    static void from_json(const json& j, std::variant<Ts...>& data)
    {
        (variant_from_json<Ts>(j, data), ...);
    }
};

// Allows serializing and deserializing contents behind a std::unique_ptr.
// unique_ptr is non_copyable, hence not recommend to use NLOHMANN_DEFINE_TYPE_INTRUSIVE in nested
// class
template <typename T>
struct adl_serializer<std::unique_ptr<T>>
{
    static void to_json(json& json_value, const std::unique_ptr<T>& ptr)
    {
        if (ptr.get())
        {
            json_value = *ptr;
        }
        else
        {
            json_value = nullptr;
        }
    }

    static void from_json(const json& json_value, std::unique_ptr<T>& ptr)
    {
        T inner_val = json_value.template get<T>();
        ptr = std::make_unique<T>(std::move(inner_val));
    }
};

// Allows serializing and deserializing contents behind a std::shared_ptr.
// shared_ptr is copyable, hence can be used in nested class by NLOHMANN_DEFINE_TYPE_INTRUSIVE
template <typename T>
struct adl_serializer<std::shared_ptr<T>>
{
    static void to_json(json& json_value, const std::shared_ptr<T>& ptr)
    {
        if (ptr.get())
        {
            json_value = *ptr;
        }
        else
        {
            json_value = nullptr;
        }
    }

    static void from_json(const json& json_value, std::shared_ptr<T>& ptr)
    {
        T inner_val = json_value.template get<T>();
        ptr = std::make_shared<T>(std::move(inner_val));
    }
};
} // namespace nlohmann

inline bool IsJsonFile(const std::string& fileName)
{
    return EndsWith(fileName, ".json");
}

struct JsonConfigParams
{
    int enableDump{}; // enableDump 配置为1表示启用json
                      // dump功能（在log中打印序列化的json文件的每一项），0表示禁用
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(JsonConfigParams, enableDump)
};

// JsonConfig: used to config json srlz/dsrlz params from json file
class JsonConfig : public Singleton<JsonConfig>
{
public:
    JsonConfig()
    {
        std::ifstream ifs(CONFIG_PARAM_PATH);
        if (!ifs.is_open())
        {
            PV("open failed", ifs.fail());
            return;
        }
        nlohmann::json j = nlohmann::json::parse(ifs, nullptr, false);
        CHECK_AND_RETURN(j.is_discarded());
        auto configParams = j.template get<JsonConfigParams>();
        m_enableDump = configParams.enableDump;
        PV(m_enableDump.load());
    }

    bool GetDumpEnable() const
    {
        return m_enableDump.load();
    }

private:
    std::atomic<int> m_enableDump{};
};

inline void Save2Json(const nlohmann::json& j, const std::string& jsonName, int i = DEFAULT_INDENT)
{
    std::ofstream ofs(jsonName);
    if (!ofs.is_open())
    {
        PV("open failed", ofs.fail());
        return;
    }
    ofs << std::setw(i) << j;
}

inline void DumpJson(const nlohmann::json& j)
{
    CHECK_AND_RETURN(!JsonConfig::GetInstance().GetDumpEnable());
    PV(j.dump(DEFAULT_INDENT));
    // LogIBySs(j.dump(DEFAULT_INDENT));
}

template <typename T>
void Srlz(const T& t, const std::string& jsonName, int i = DEFAULT_INDENT)
{
    CHECK_AND_RETURN(!IsJsonFile(jsonName));
    nlohmann::json j = t;
    Save2Json(j, jsonName, DEFAULT_INDENT);
    DumpJson(j);
}

template <typename T>
std::optional<T> Dsrlz(const std::string& jsonName)
{
    std::ifstream ifs(jsonName);
    if (!ifs.is_open())
    {
        PV("open failed", ifs.fail());
        return {};
    } // check file exist
    CHECK_AND_RETURN_RET(!IsJsonFile(jsonName), {});        // check file name is json;
    CHECK_AND_RETURN_RET(!nlohmann::json::accept(ifs), {}); // check well-json-formating file
    ifs.seekg(0); // json::accept() would set ifs to last, hence need reset ifs to begin;
    nlohmann::json j = nlohmann::json::parse(ifs, nullptr, false);
    CHECK_AND_RETURN_RET(j.is_discarded(), {}); // check valid json file parsed result success
    auto t = j.template get<T>();
    DumpJson(j);
    return t;
}

#endif // DEBUG

#endif // JSONUTILS_H

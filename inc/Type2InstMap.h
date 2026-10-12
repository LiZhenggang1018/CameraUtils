/*
 * @Description: Type2InstMap Impl which can expand Singleton to Singleton Pool
 */

#ifndef TYPE2INSTMAP_H
#define TYPE2INSTMAP_H

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "CustomLog.h"
#include "Singleton.h"

/*
* user sample code:

* using ReqMgrMap = Type2InstMap<IVEReqType, RequestManager>;   // header file define ReqMgrMap

* const vector<IVEReqType> toRegisterIveReqTypes {IVEReqType::CAP, IVEReqType::VID};   // source
file
* ReqMgrMap::Register(toRegisterIveReqTypes);  // Register instances ReqMgrMap first
* ReqMgrMap::GetMgr(IVEReqType::CAP).RemoveRequestData();  // use ReqMgrMap later
*/

template <typename Type, typename Inst>
class Type2InstMap : public Singleton<Type2InstMap<Type, Inst>>
{
public:
    static void Register(const std::vector<Type>& types)
    {
        static_assert(std::is_constructible_v<Inst, Type> || std::is_constructible_v<Inst>);
        if constexpr (std::is_constructible_v<Inst, Type>)
        {
            for (auto&& type : types)
            {
                Dump("Register", type);
                if (auto&& [pos, ret] = m_mgrMap.emplace(std::piecewise_construct,
                                                         std::forward_as_tuple(type),
                                                         std::forward_as_tuple(type));
                    !ret)
                {
                    Dump("map insert error", type);
                }
            }
        }
        else if constexpr (!std::is_constructible_v<Inst, Type> && std::is_constructible_v<Inst>)
        {
            for (auto&& type : types)
            {
                Dump("Register", type);
                if (auto&& [pos, ret] = m_mgrMap.emplace(std::piecewise_construct,
                                                         std::forward_as_tuple(type),
                                                         std::forward_as_tuple());
                    !ret)
                {
                    Dump("map insert error", type);
                }
            }
        }
    }

    static decltype(auto) GetMgr(Type type)
    {
        if (Type2InstMap::GetInstance().m_mgrMap.find(type) ==
            Type2InstMap::GetInstance().m_mgrMap.end())
        {
            Dump("GetMgr error", type);
        }
        return Type2InstMap::GetInstance().m_mgrMap.at(type);
    }

    // AMgrMap::ForEachMgr([&](auto& a) {a.MethodInA();});
    static void ForEachMgr(std::function<void(Inst&)> cb)
    {
        for (auto&& [type, inst] : Type2InstMap::GetInstance().m_mgrMap)
        {
            cb(inst);
        }
        return;
    }

private:
    static std::optional<std::string> ConvertType2Str(const Type& type)
    {
        // enum's is_convertible_v<Type, int> is false;
        if constexpr (std::is_convertible_v<Type, int> || std::is_enum_v<Type>)
        {
            auto i = static_cast<int>(type);
            return std::to_string(i);
        }
        else if constexpr (std::is_convertible_v<Type, std::string>)
        {
            return static_cast<std::string>(type);
        }
        else
        {
            return {};
        }
    }

    static void Dump(const std::string& str, const Type& type)
    {
        auto op = ConvertType2Str(type);
        if (op)
        {
            PV(str, "at idx", *op);
        }
        else
        {
            PV(str);
        }
    }
    inline static std::unordered_map<Type, Inst> m_mgrMap{};
};

#endif // TYPE2INSTMAP_H

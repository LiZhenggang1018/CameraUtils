#ifndef GENERALFACTORY_H
#define GENERALFACTORY_H

#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

// 通用工厂模式（General
// Factory），允许根据一个键（通常是字符串）动态创建实现了某个接口（Intf）的派生类对象。

namespace detail
{

// CreatorTraits: 根据 Intf（接口类型）、KType（键类型）、PassName（是否传递名称）以及实际构造参数
// Args...，生成一个创建器（Creator）。 创建器是一个可调用对象，接受 Args... 参数，返回
// std::shared_ptr<Intf>。
template <typename Intf, typename KType, bool PassName, typename = void>
struct CreatorTraits
{
    using type = std::shared_ptr<Intf>;

    template <typename... Args>
    struct CreatorGenerator
    {
        using CreatorType = std::function<type(Args...)>;

        template <typename U>
        CreatorType GetCreator(const KType& name)
        {
            static_assert(std::is_base_of_v<Intf, U>,
                          "U must be derived from Intf"); // U必须是Intf的派生类
            if constexpr (PassName)
            {
                return [name](auto&&... args) -> type
                {
                    return std::make_shared<U>(
                        name,
                        std::forward<decltype(args)>(
                            args)...); // 当 PassName == true 时，U的构造函数必须接受 (KType name,
                                       // Args... args) 形式的参数。
                };
            }
            else
            {
                return [](auto&&... args) -> type
                {
                    return std::make_shared<U>(std::forward<decltype(args)>(
                        args)...); // 当 PassName == false 时，U的构造函数必须接受 (Args... args)
                                   // 形式的参数。
                };
            }
        }
    };
};

// Intf：接口类型（所有产品必须派生自它）。
// KType：键类型，默认为 std::string。
// PassName：是否将注册键传递给构造函数，默认为 true。
// Args...：构造函数的额外参数类型包（不包含可能存在的名称参数）。
template <typename Intf, typename KType = std::string, bool PassName = true, typename... Args>
class GeneralFactory
{
    using CreatorGenerator =
        typename CreatorTraits<Intf, KType, PassName>::template CreatorGenerator<Args...>;
    using CreatorType = typename CreatorGenerator::CreatorType;
    using ProductPtr = std::shared_ptr<Intf>;

    // 编译期断言：不支持工厂的 Args...类型中包含 非const的左值引用（非const的T&）。 //
    // 目前不支持，后期待定 TBD：这里待确认 当构造函数包含非const的 T&形参时 是否支持
    static_assert((... && (!std::is_lvalue_reference_v<Args> ||
                           std::is_const_v<std::remove_reference_t<Args>>)),
                  "GeneralFactory: non-const lvalue reference types in Args... are unsupported; "
                  "use value or const& instead.");

public:
    // 提供通用的单例实现；如果某个使用方需要自定义单例策略，可自行提供显式特化并移除或修改此实现。
    static GeneralFactory& Instance()
    {
        static GeneralFactory instance;
        return instance;
    }

    // 创建单个对象
    template <typename... TArgs>
    ProductPtr Create(const KType& name, TArgs&&... args)
    {
        std::lock_guard<std::mutex> lk(m_creatorTblMutex);
        auto creatorIter = m_creatorTbl.find(name);
        if (creatorIter == m_creatorTbl.end())
        {
            std::stringstream kname;
            kname << name;
            // LOGE("create %s not found!", kname.str().c_str());
            return {};
        }
        return creatorIter->second(std::forward<TArgs>(args)...);
    }

    // 创建所有注册的对象
    template <typename... TArgs>
    std::vector<ProductPtr> CreateAll(TArgs&&... args)
    {
        std::lock_guard<std::mutex> lk(m_creatorTblMutex);
        std::vector<ProductPtr> typeVec{};
        for (const auto& pair : m_creatorTbl)
        {
            auto obj = pair.second(std::forward<TArgs>(args)...);
            if (obj != nullptr)
            {
                std::stringstream kname;
                kname << pair.first;
                // LOGI("create %s success", kname.str().c_str());
                typeVec.push_back(obj);
            }
        }
        return typeVec;
    }

    template <typename U>
    int RegisterCreator(const KType& name)
    {
        std::lock_guard<std::mutex> lk(m_creatorTblMutex);
        CheckNameExists(name);
        m_creatorTbl.emplace(name, CreatorGenerator{}.template GetCreator<U>(name));
        return 0;
    }

    bool CheckNameExists(const KType& name) const
    {
        return m_creatorTbl.count(name) != 0;
    }

private:
    void CheckNameExists(const KType& name)
    {
        std::stringstream kname;
        kname << name;
        if (m_creatorTbl.count(name) != 0)
        {
            // LOGE("found same type! type=%s", kname.str().c_str());
        }
    }

private:
    mutable std::mutex m_creatorTblMutex;
    std::unordered_map<KType, CreatorType> m_creatorTbl;
};

} // namespace detail

// 控制运行 FactoryDemo.h 还是 FactoryDemo_1.h
// #define TEST_FACTORY_DEMO 1 // 定义宏以启用 FactoryDemo.h
// 中的测试代码；注释掉此行则运行FactoryDemo_1.h。不注释则运行FactoryDemo.h

#endif // GENERALFACTORY_H
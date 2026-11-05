#ifndef INC_ADVANCEDLIBWRAPPER_H_
#define INC_ADVANCEDLIBWRAPPER_H_

#include "CommonEnv.h"
#if CAMERAUTILS_HAS_DLFCN
#include "LibWrapper.hpp"
#include <functional>
#include <unordered_map>

class AdvancedLibWrapper : public LibWrapper {
public:
    using LibWrapper::LibWrapper;

    // 类型安全的函数包装器
    template<typename Ret, typename... Args>
    std::function<Ret(Args...)> GetFunctionSafe(const std::string& functionName) {
        using FuncType = Ret(*)(Args...);
        auto funcPtr = GetFunction<FuncType>(functionName);
        return [funcPtr](Args... args) -> Ret {
            return funcPtr(args...);
            };
    }

    // 缓存函数指针以提高性能
    template<typename FuncPtr>
    FuncPtr GetCachedFunction(const std::string& functionName) {
        auto it = m_functionCache.find(functionName);
        if (it != m_functionCache.end()) {
            return reinterpret_cast<FuncPtr>(it->second);
        }

        void* symbol = dlsym(GetHandle(), functionName.c_str());
        CARR(!symbol, nullptr);
        m_functionCache[functionName] = symbol;
        return reinterpret_cast<FuncPtr>(symbol);
    }

private:
    void* GetHandle() const {
        // 在实际实现中需要通过适当的方式访问基类的句柄
        // 这里为了简化，返回nullptr，实际使用时需要修改
        return nullptr;
    }

    std::unordered_map<std::string, void*> m_functionCache;
};

#endif // CAMERAUTILS_HAS_DLFCN

#endif // INC_ADVANCEDLIBWRAPPER_H_

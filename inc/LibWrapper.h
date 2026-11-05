#ifndef INC_LIBWRAPPER_H_
#define INC_LIBWRAPPER_H_

#include "CommonEnv.h"
#if CAMERAUTILS_HAS_DLFCN
#include <dlfcn.h>

#include <memory>
#include <string>
#include <type_traits>
#include <stdexcept>
#include <iostream>
#include "CheckUtils.h"

class LibWrapper {
public:
    explicit LibWrapper(const std::string& libraryPath, int flags = RTLD_LAZY)
    {
        m_handle = dlopen(libraryPath.c_str(), flags);
        CAR(!m_handle);
    }

    ~LibWrapper() {
        if (m_handle) {
            dlclose(m_handle);
        }
    }

    // 禁止拷贝
    LibWrapper(const LibWrapper&) = delete;
    LibWrapper& operator=(const LibWrapper&) = delete;

    // 允许移动
    LibWrapper(LibWrapper&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    LibWrapper& operator=(LibWrapper&& other) noexcept {
        if (this != &other) {
            if (m_handle) {
                dlclose(m_handle);
            }
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    // 获取函数指针
    template<typename FuncPtr>
    FuncPtr GetFunction(const std::string& functionName) {
        void* symbol = dlsym(m_handle, functionName.c_str());
        CARR(!symbol, nullptr);
        // 类型安全的转换
        return reinterpret_cast<FuncPtr>(symbol);
    }

    // 便捷调用函数（适用于无返回值函数）,auto不可用于推导返回值为非完整类型void的case
    template<typename FuncPtr, typename... Args>
    void CallVoid(const std::string& functionName, Args&&... args) {
        auto func = GetFunction<FuncPtr>(functionName);
        func(std::forward<Args>(args)...);
    }

    // 便捷调用函数（适用于有返回值函数）
    template<typename FuncPtr, typename... Args>
    auto Call(const std::string& functionName, Args&&... args)
        -> std::invoke_result_t<FuncPtr, Args...> {
        auto func = GetFunction<FuncPtr>(functionName);
        return func(std::forward<Args>(args)...);
    }

    // 检查库是否加载成功
    bool IsLoaded() const noexcept {
        return m_handle != nullptr;
    }

    // 获取最后错误信息
    std::string GetLastError() const {
        const char* error = dlerror();
        return error ? std::string(error) : std::string();
    }

private:
    void* m_handle{};
};

#endif // CAMERAUTILS_HAS_DLFCN

#endif // INC_LIBWRAPPER_H_
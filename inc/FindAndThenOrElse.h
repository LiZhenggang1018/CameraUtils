#ifndef FIND_AND_THEN_OR_ELSE_H
#define FIND_AND_THEN_OR_ELSE_H

#include <algorithm>
#include <deque>
#include <iterator>
#include <list>
#include <optional>
#include <type_traits>
#include <vector>

// 实现序列容器查找结果的AndThen与OrElse的链式调用；

// 判断一个类型是否是序列容器的布尔型模板变量
template <typename T>
constexpr bool is_sequence_container_v =
    // 检查类型 T 是否定义了 iterator 和 const_iterator 类型。序列容器（如
    // std::vector、std::list、std::deque）通常都定义了这两个类型。
    std::is_same_v<typename T::iterator, typename T::iterator> &&
    std::is_same_v<typename T::const_iterator, typename T::const_iterator> &&
    // 检查类型 T 是否定义了 allocator_type，并且该类型是否为标准分配器 std::allocator。确保 T
    // 是使用标准分配器的顺序容器。
    (std::is_same_v<typename T::allocator_type, std::allocator<typename T::value_type>> ||
     std::is_same_v<typename T::allocator_type, std::allocator<typename T::value_type>>);

template <typename T>
class FindResult
{
public:
    FindResult() = default;
    explicit FindResult(T value) : m_value{value}
    {
    }

    // AndThen()接受一个回调函数
    template <typename Func>
    const FindResult<T>& AndThen(Func&& func) const
    {
        // 只有当我们有值时才调用回调函数
        if (m_value.has_value())
        {
            std::forward<Func>(func)(m_value.value());
        }
        return *this;
    }

    // OrElse()与AndThen()几乎相同的实现，只是返回类型为 void
    template <typename Func>
    void OrElse(Func&& func) const
    {
        if (!m_value.has_value())
        {
            std::forward<Func>(func)();
        }
    }

private:
    // 不确定是否找到了值使用 optional存储可能的搜索结果
    std::optional<T> m_value{std::nullopt};
};
// 查找函数，用于在在顺序容器中查找值
template <typename Container, typename T>
auto Find(const Container& c, const T& value)
{
    static_assert(is_sequence_container_v<Container>, "Container must be a sequence container");
    auto it = std::find(std::begin(c), std::end(c), value);
    // 如果没有找到值，返回默认构造的 find_result
    if (it == std::end(c))
    {
        return FindResult<T>();
    }
    else
    {
        // 否则返回找到的值
        return FindResult<T>(*it);
    }
}

#endif // FIND_AND_THEN_OR_ELSE_H

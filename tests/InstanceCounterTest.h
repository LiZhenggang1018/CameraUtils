#ifndef INSTANCECOUNTERTEST_H
#define INSTANCECOUNTERTEST_H

#include "InstanceCounter.h"

// 示例类A
class CounterA : public Counter<CounterA>
{
public:
    CounterA() = default;
    CounterA(const CounterA&) = default;
    CounterA(CounterA&&) = default;

    void print() const
    {
        std::cout << "A对象地址: " << this << std::endl;
    }
};

// 示例类B
class CounterB : public Counter<CounterB>
{
public:
    CounterB() = default;
    CounterB(const CounterB&) = default;
    CounterB(CounterB&&) = default;

    void show() const
    {
        std::cout << "B对象地址: " << this << std::endl;
    }
};

// 测试函数
inline void testFunction()
{
    std::cout << "\n=== 进入testFunction ===" << std::endl;

    CounterA a3;
    CounterB b2;

    std::cout << "函数内 - A实例数: " << CounterA::GetInstanceCount()
              << ", B实例数: " << CounterB::GetInstanceCount() << std::endl;

    std::cout << "=== 离开testFunction ===" << std::endl;
}

inline void InstanceCounterTest()
{
    std::cout << "=== 程序开始 ===" << std::endl;

    // 初始状态
    std::cout << "初始 - A实例数: " << CounterA::GetInstanceCount()
              << ", B实例数: " << CounterB::GetInstanceCount() << std::endl;

    // 创建一些对象
    CounterA a1;
    CounterA a2;
    CounterB b1;

    std::cout << "创建后 - A实例数: " << CounterA::GetInstanceCount()
              << ", B实例数: " << CounterB::GetInstanceCount() << std::endl;

    // 测试拷贝构造
    std::cout << "\n--- 测试拷贝构造 ---" << std::endl;
    CounterA a_copy = a1;

    // 测试移动构造
    std::cout << "\n--- 测试移动构造 ---" << std::endl;
    CounterA a_move = std::move(a2);

    std::cout << "\n拷贝/移动后 - A实例数: " << CounterA::GetInstanceCount()
              << ", B实例数: " << CounterB::GetInstanceCount() << std::endl;

    // 测试函数作用域
    testFunction();

    // 动态创建对象
    std::cout << "\n--- 测试动态创建 ---" << std::endl;
    CounterA* dynamicA = new CounterA();
    CounterB* dynamicB = new CounterB();

    std::cout << "动态创建后 - A实例数: " << CounterA::GetInstanceCount()
              << ", B实例数: " << CounterB::GetInstanceCount() << std::endl;

    delete dynamicA;
    delete dynamicB;

    std::cout << "动态删除后 - A实例数: " << CounterA::GetInstanceCount()
              << ", B实例数: " << CounterB::GetInstanceCount() << std::endl;

    std::cout << "\n=== 程序结束 ===" << std::endl;
    return;
}

#endif // INSTANCECOUNTERTEST_H
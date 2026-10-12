#ifndef FACTORY_DEMO_H
#define FACTORY_DEMO_H

#include "AllUsedHeadFiles.h"

#include "GeneralFactory.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifdef TEST_FACTORY_DEMO

// 1. 定义接口 Shape
class Shape
{
public:
    virtual void draw() const = 0;
    virtual ~Shape() = default;
};

// 2. 具体类：Circle（构造函数按值接收 name radius）
class Circle : public Shape
{
    std::string m_name;
    double m_radius;

public:
    Circle(std::string name, double radius) : m_name(std::move(name)), m_radius(radius)
    {
    }
    void draw() const override
    {
        std::cout << "Circle[" << m_name << "] with radius " << m_radius << std::endl;
    }
};

// 具体类：Square（按值接收 name）
class Square : public Shape
{
    std::string m_name;
    double m_side;

public:
    Square(std::string name, double side) : m_name(std::move(name)), m_side(side)
    {
    }
    void draw() const override
    {
        std::cout << "Square[" << m_name << "] with side " << m_side << std::endl;
    }
};

// 定义工厂类型（接口 Shape，键类型 string，传递名称，额外参数 double）
using ShapeFactory = detail::GeneralFactory<Shape, std::string, true, double>;

// 定义自动注册宏
#define REGISTER_SHAPE_BY_NAME(_clsName, _cls) \
    static int g_shape_##_clsName##_cls = ShapeFactory::Instance().RegisterCreator<_cls>(#_clsName)
#define REGISTER_SHAPE(_cls) REGISTER_SHAPE_BY_NAME(_cls, _cls)

// 使用宏自动注册 Circle 和 Square（在全局作用域）
REGISTER_SHAPE(Circle); // 等价于 RegisterCreator<Circle>("Circle")
REGISTER_SHAPE(Square); // 等价于 RegisterCreator<Square>("Square")

inline void FactoryTest()
{
    auto& factory = ShapeFactory::Instance();

    // 创建对象（使用注册时的名称）
    auto shape1 = factory.Create("Circle", 5.0);
    auto shape2 = factory.Create("Square", 3.0);
    auto shape3 = factory.Create("Triangle", 2.0); // 未注册,打印报错

    if (shape1)
    {
        shape1->draw();
    }
    if (shape2)
    {
        shape2->draw();
    }
    if (!shape3)
    {
        std::cout << "Triangle not registered\n";
    }

    // 批量创建所有已注册的类型（参数 4.0 传递给构造函数）
    auto allShapes = factory.CreateAll(4.0);
    for (auto&& s : allShapes)
    {
        s->draw();
    }

    return;
}
#endif

#endif // FACTORY_DEMO_H

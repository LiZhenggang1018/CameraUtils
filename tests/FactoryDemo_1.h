#ifndef FACTORY_DEMO_1_H
#define FACTORY_DEMO_1_H

#include "AllUsedHeadFiles.h"

#include "GeneralFactory.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

// FactoryDemo_1.h跟 FactoryDemo.h几乎一样，展示了不将注册键传递给构造函数的用法（PassName =
// false）。 在这种情况下，Circle 和 Square
// 的构造函数不接受std::string类型的名称参数，而是直接接受构造参数（如double类型的 radius 或
// side）。工厂创建对象时只传递构造参数，而不传递注册键。

#ifndef TEST_FACTORY_DEMO

// 接口 Shape
class Shape
{
public:
    virtual void draw() const = 0;
    virtual ~Shape() = default;
};

// 具体类：Circle（构造函数只接受 radius）
class Circle : public Shape
{
    std::string m_name;
    double m_radius;

public:
    explicit Circle(double radius) : m_name("Circle"), m_radius(radius)
    {
    } // 构造函数不接受名称参数
    void draw() const override
    {
        std::cout << "Circle[" << m_name << "] with radius " << m_radius << std::endl;
    }
};

// 具体类：Square（构造函数只接受 side）
class Square : public Shape
{
    std::string m_name;
    double m_side;

public:
    explicit Square(double side) : m_name("Square"), m_side(side)
    {
    }
    void draw() const override
    {
        std::cout << "Square[" << m_name << "] with side " << m_side << std::endl;
    }
};

// 需要额外增加的宏定义：
//  1、定义工厂类型：不将注册键传递给构造函数（PassName = false），额外参数 double
using ShapeFactory = detail::GeneralFactory<Shape, std::string, false, double>;

// 2、定义自动注册宏（保留以字符串键注册映射）
#define REGISTER_SHAPE_BY_NAME(_clsName, _cls) \
    static int g_shape_##_clsName##_cls = ShapeFactory::Instance().RegisterCreator<_cls>(#_clsName)
#define REGISTER_SHAPE(_cls) REGISTER_SHAPE_BY_NAME(_cls, _cls)

// 3、使用宏自动注册 Circle 和 Square（在全局作用域）
REGISTER_SHAPE(Circle); // 注册键："Circle"
REGISTER_SHAPE(Square); // 注册键："Square"

inline void FactoryTest()
{
    auto& factory = ShapeFactory::Instance();

    // 创建对象（使用注册时的名称作为键，构造参数只传 double）
    auto shape1 = factory.Create("Circle", 5.0);
    auto shape2 = factory.Create("Square", 3.0);
    auto shape3 = factory.Create("Triangle", 2.0); // 未注册

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

#endif // TEST_FACTORY_DEMO

#endif // FACTORY_DEMO_1_H

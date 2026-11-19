#ifndef TESTS_FACTORYDEMO_1_H
#define TESTS_FACTORYDEMO_1_H

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
class Shape {
public:
    virtual void Draw() const = 0;
    virtual ~Shape() = default;
};

// 具体类：Circle（构造函数只接受 radius）
class Circle : public Shape {
    std::string name_;
    double radius_;

public:
    explicit Circle(double radius)
        : name_("Circle"),
          radius_(radius)
    {} // 构造函数不接受名称参数
    void Draw() const override { std::cout << "Circle[" << name_ << "] with radius " << radius_ << std::endl; }
};

// 具体类：Square（构造函数只接受 side）
class Square : public Shape {
    std::string name_;
    double side_;

public:
    explicit Square(double side)
        : name_("Square"),
          side_(side)
    {}
    void Draw() const override { std::cout << "Square[" << name_ << "] with side " << side_ << std::endl; }
};

// 需要额外增加的宏定义：
//  1、定义工厂类型：不将注册键传递给构造函数（PassName = false），额外参数 double
using ShapeFactory = Detail::GeneralFactory<Shape, std::string, false, double>;

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

    if (shape1) {
        shape1->Draw();
    }
    if (shape2) {
        shape2->Draw();
    }
    if (!shape3) {
        std::cout << "Triangle not registered\n";
    }

    // 批量创建所有已注册的类型（参数 4.0 传递给构造函数）
    auto allShapes = factory.CreateAll(4.0);
    for (auto&& s : allShapes) {
        s->Draw();
    }
}

#endif // TEST_FACTORY_DEMO

#endif // TESTS_FACTORYDEMO_1_H

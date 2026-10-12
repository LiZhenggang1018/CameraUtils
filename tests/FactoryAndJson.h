#ifndef FACTORY_AND_JSON_H
#define FACTORY_AND_JSON_H

#include "JsonUtils.h"

#include "FactoryDemo_1.h"

inline const std::string JsonPath = R"(configs\Shapes.json)";

// 用于生成json文件，无用
inline void GenJson()
{
    std::vector<std::string> strs{"Circle", "Square"};
    Srlz(strs, JsonPath);
}

// 从 1.json文件中自动加载各子类名，如 "Circle", "Square"，并创建各子类，调用子类的draw()方法。
inline void FactoryJsonTest()
{

    std::vector<std::string> names{};
    auto p = Dsrlz<std::vector<std::string>>(JsonPath); // 从json文件中解析出子类的名称，存入names
    if (p)                                              // 判断反序列化是否成功
    {
        names = *p;
    }

    for (const auto& name : names)
    {
        auto inst = ShapeFactory::Instance().Create(name, 5.0); // 根据子类名 依次创建各子类
        if (inst)
        {
            inst->draw();
        }
    }

    return;
}

#endif // !FACTORY_AND_JSON_H
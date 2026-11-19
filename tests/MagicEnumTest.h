#ifndef TESTS_MAGICENUMTEST_H
#define TESTS_MAGICENUMTEST_H

#include <iostream>
#include <string>
#include "MagicEnumCommon.h"
#include "cli/TestAssert.h"

namespace MagicEnumTest {
// 定义一个测试用枚举
enum class Color : int { RED = -10, BLUE = 0, GREEN = 10 };

inline void TestMagicEnum()
{
    std::cout << "===== magic_enum test begin =====" << std::endl;

    // 1. enum -> string
    Color c1 = Color::RED;
    std::string name = std::string(magic_enum::enum_name(c1));
    std::cout << "enum_name(Color::RED) = " << name << std::endl;
    CU_CHECK(name == "RED");

    // 2. string -> enum
    std::string colorName = "GREEN";
    auto colorOpt = magic_enum::enum_cast<Color>(colorName);
    CU_CHECK(colorOpt.has_value());
    CU_CHECK(colorOpt.value() == Color::GREEN);
    std::cout << "enum_cast(\"GREEN\") -> Color::GREEN OK" << std::endl;

    // 3. 遍历枚举所有值
    std::cout << "all Color values: ";
    for (auto color : magic_enum::enum_values<Color>()) {
        std::cout << magic_enum::enum_name(color) << " ";
    }
    std::cout << std::endl;

    // 4. enum_count
    CU_CHECK(magic_enum::enum_count<Color>() == 3);
    std::cout << "enum_count<Color>() = 3 OK" << std::endl;

    // 5. 带默认值的 enum_cast（值不存在时）
    auto invalid = magic_enum::enum_cast<Color>(999).value_or(Color::BLUE);
    CU_CHECK(invalid == Color::BLUE);
    std::cout << "enum_cast(999).value_or(BLUE) -> BLUE OK" << std::endl;

    std::cout << "===== magic_enum test passed =====" << std::endl;
}
} // namespace MagicEnumTest

#endif // TESTS_MAGICENUMTEST_H

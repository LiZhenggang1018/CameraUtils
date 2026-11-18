#ifndef TESTS_MAGICENUMTEST_H_
#define TESTS_MAGICENUMTEST_H_

#include "MagicEnumCommon.h"
#include "cli/TestAssert.h"
#include <iostream>
#include <string>

namespace MagicEnumTest
{
// 瀹氫箟涓€涓祴璇曠敤鏋氫妇
enum class Color : int
{
    RED = -10,
    BLUE = 0,
    GREEN = 10
};

inline void TestMagicEnum()
{
    std::cout << "===== magic_enum test begin =====" << std::endl;

    // 1. enum -> string
    Color c1 = Color::RED;
    std::string name = std::string(magic_enum::enum_name(c1));
    std::cout << "enum_name(Color::RED) = " << name << std::endl;
    CU_CHECK(name == "RED");

    // 2. string -> enum
    std::string color_name = "GREEN";
    auto color_opt = magic_enum::enum_cast<Color>(color_name);
    CU_CHECK(color_opt.has_value());
    CU_CHECK(color_opt.value() == Color::GREEN);
    std::cout << "enum_cast(\"GREEN\") -> Color::GREEN OK" << std::endl;

    // 3. 閬嶅巻鏋氫妇鎵€鏈夊€?
    std::cout << "all Color values: ";
    for (auto color : magic_enum::enum_values<Color>())
    {
        std::cout << magic_enum::enum_name(color) << " ";
    }
    std::cout << std::endl;

    // 4. enum_count
    CU_CHECK(magic_enum::enum_count<Color>() == 3);
    std::cout << "enum_count<Color>() = 3 OK" << std::endl;

    // 5. 甯﹂粯璁ゅ€肩殑 enum_cast锛堝€间笉瀛樺湪鏃讹級
    auto invalid = magic_enum::enum_cast<Color>(999).value_or(Color::BLUE);
    CU_CHECK(invalid == Color::BLUE);
    std::cout << "enum_cast(999).value_or(BLUE) -> BLUE OK" << std::endl;

    std::cout << "===== magic_enum test passed =====" << std::endl;
}
} // namespace MagicEnumTest

#endif // TESTS_MAGICENUMTEST_H_

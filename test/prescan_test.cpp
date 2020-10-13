
#include "environment/prescan.h"
#include "mls-unit-test/unittest.h"
#include <sstream>

TEST_SUIT_BEGIN

TEST_CASE("find all includes") {
    const std::string s = R"_(
# 1 "src/matmake-common.h" 1

# 1 "src/token.h" 1
# 1 "<built-in>" 1
# 1 "<built-in>" 3
# 398 "<built-in>" 3
# 1 "<command line>" 1
# 1 "<built-in>" 2

# 1 "/usr/include/c++/9/algorithm" 1 3
# 58 "/usr/include/c++/9/algorithm" 3

# 59 "/usr/include/c++/9/algorithm" 3

# 1 "/usr/include/c++/9/utility" 1 3
         )_";

    std::istringstream ss(s);

    auto res = prescan(ss);

    ASSERT_GT(res.includes.size(), 0);
    ASSERT_GT(res.systemHeaders.size(), 0);

    for (auto include : res.includes) {
        std::cout << include << "\n";
    }

    std::cout << "\nsystem headers:\n";

    for (auto include : res.systemHeaders) {
        std::cout << include << "\n";
    }
}

TEST_CASE("prescan module") {
    const std::string s = R"_(
# 1 "mod1.cppm"
# 1 "<built-in>" 1
# 1 "<built-in>" 3
# 408 "<built-in>" 3
# 1 "<command line>" 1
# 1 "<built-in>" 2
# 1 "mod1.cppm" 2

module;

export module mod1;

import mod2;
# 17 "mod1.cppm"
namespace mod1 {

export int getNum() {
    return mod2::getNum2();
}

}

)_";
    std::istringstream ss(s);

    auto res = prescan(ss);

    ASSERT_GT(res.imports.size(), 0);
    ASSERT_EQ(res.imports.front(), "mod2");

    for (auto line : res.imports) {
        std::cout << line << "\n";
    }

    ASSERT_GT(res.exportModules.size(), 0);
    ASSERT_EQ(res.exportModules.front(), "mod1");

    std::cout << "\nexports:\n";
    for (auto line : res.exportModules) {
        std::cout << line << "\n";
    }
}

TEST_SUIT_END

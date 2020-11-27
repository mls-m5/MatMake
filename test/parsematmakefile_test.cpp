#include "mls-unit-test/unittest.h"
#include "main/parsematmakefile.h"
#include "mocks/mockifiles.h"
#include <sstream>
#include <fstream>

const auto test1 = R"_(
src = *.cpp
out = main
debug: dir=bin

)_";

TEST_SUIT_BEGIN

TEST_CASE("parse basic") {
    std::istringstream ss(test1);

    MockIFiles files;

    files.mock_openRead_1.onCall([&ss](auto &&){return fileFromSs(ss);});

    Locals locals;

    auto [shouldQuit, isError, properties] = parseMatmakeFile(locals, files);

    ASSERT_EQ(properties.size(), 1);

    auto &root = properties.front();

    ASSERT_EQ((*root)["src"].concat(), "*.cpp");
    ASSERT_EQ((*root)["out"].concat(), "main");
    ASSERT_NE((*root)["dir"].concat(), "bin"); // Conditioned out

}

TEST_CASE("parse basic with condition") {
    std::istringstream ss(test1);

    MockIFiles files;

    files.mock_openRead_1.onCall([&ss](auto &&){return fileFromSs(ss);});

    Locals locals;

    locals.config = "debug";

    auto [shouldQuit, isError, properties] = parseMatmakeFile(locals, files);

    ASSERT_EQ(properties.size(), 1);

    auto &root = properties.front();

    ASSERT_EQ((*root)["src"].concat(), "*.cpp");
    ASSERT_EQ((*root)["out"].concat(), "main");
    ASSERT_EQ((*root)["dir"].concat(), "bin");
}

TEST_SUIT_END

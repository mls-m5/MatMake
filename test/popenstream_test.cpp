
#include "environment/popenstream.h"
#include "mls-unit-test/unittest.h"

TEST_SUIT_BEGIN

TEST_CASE("run echo") {
    POpenStream pstream("echo hello");

    std::string line;

    getline(pstream, line);
    ASSERT_EQ(line, "hello");
}

TEST_CASE("run multiple lines") {
    POpenStream pstream("echo \"hello\\nthere\"");

    std::string line;

    getline(pstream, line);
    ASSERT_EQ(line, "hello");

    getline(pstream, line);
    ASSERT_EQ(line, "there");
}

TEST_SUIT_END

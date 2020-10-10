
#include "environment/files.h"
#include "environment/globals.h"
#include "main/token.h"
#include "mls-unit-test/unittest.h"
#include <experimental/filesystem>
#include <fstream>

using namespace std;
namespace filesystem = experimental::filesystem;

TEST_SUIT_BEGIN

TEST_CASE("Popen") {
    Files files;
    auto result = files.popenWithResult("echo hej");
    ASSERT_EQ(result.first, 0);
    ASSERT_EQ(result.second, "hej\n");
}

TEST_CASE("time") {
    Files files;

    std::string folderName = "sandbox" + pathSeparator + "test1";
    std::string fileName = folderName + pathSeparator + "testfile";

    try {
        filesystem::remove_all(folderName);
    }
    catch (filesystem::filesystem_error &) {
        // Probably just that the folder does not exist, which is normal
    }

    filesystem::create_directories(folderName);

    {
        auto result = files.getTimeChanged(fileName);
        ASSERT_EQ(result, 0);
    }
    {
        ofstream testfile(fileName);
        testfile << "hej" << endl;
    }

    {
        auto result = files.getTimeChanged(fileName);
        ASSERT_NE(result, 0);
    }

    filesystem::remove_all(folderName);
}

TEST_CASE("match_files") {
    using std::string;
    string folderName = joinPaths("sandbox", "match_test");
    string fileName = joinPaths(folderName, "test1.txt");
    string excludeFilename = joinPaths(folderName, "exclude.bin");
    string fileGlobWithEnding = joinPaths(folderName, "*.txt");
    string fileGlobWithoutEnding = joinPaths(folderName, "*");
    string folderToExclude = joinPaths(folderName, "testtfolder");

    try {
        filesystem::remove_all(folderName);
    }
    catch (filesystem::filesystem_error &) {
        //
    }

    filesystem::create_directories(folderName);
    filesystem::create_directory(folderToExclude);

    {
        ofstream testfile(fileName);
        testfile << "hello" << endl;
        ofstream excludeFile(excludeFilename);
        excludeFile << "bye" << endl;
    }

    Files files;
    {
        auto found = files.findFiles(fileName);
        ASSERT_EQ(found.size(), 1);
        if (found.size()) {
            ASSERT_EQ(found.front(), fileName);
        }
    }
    {
        auto found = files.findFiles(fileGlobWithEnding);
        ASSERT_EQ(found.size(), 1);
        if (found.size()) {
            ASSERT_EQ(found.front(), fileName);
        }
    }
    {
        auto found = files.findFiles(fileGlobWithoutEnding);

        ASSERT_EQ(found.size(), 2);
    }

    filesystem::remove_all(folderName);
}

TEST_CASE("remove_dots") {
    {
        auto result = removeDoubleDots("../..");
        ASSERT_EQ(result, "_/_");
    }

    {
        auto result = removeDoubleDots("..");
        ASSERT_EQ(result, "_");
    }
}

TEST_SUIT_END

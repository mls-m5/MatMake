
#include "token.h"
#include "files.h"
#include "globals.h"
#include "gtest/gtest.h"
#include <fstream>
#include <experimental/filesystem>

using namespace std;
namespace filesystem = experimental::filesystem;

namespace {


TEST(Files, Popen) {
	Files files;
	auto result = files.popenWithResult("echo hej");
	EXPECT_EQ(result.first, 0);
	EXPECT_EQ(result.second, "hej\n");
}


TEST(Files, time) {
	Files files;

	std::string folderName = "sandbox" + pathSeparator + "test1";
	std::string fileName = folderName + pathSeparator + "testfile";

	try {
		filesystem::remove_all(folderName);
	} catch (filesystem::filesystem_error &) {
		// Probably just that the folder does not exist, which is normal
	}

	filesystem::create_directories(folderName);

	{
		auto result = files.getTimeChanged(fileName);
		EXPECT_EQ(result, 0);
	}
	{
		ofstream testfile(fileName);
		testfile << "hej" << endl;
	}

	{
		auto result = files.getTimeChanged(fileName);
		EXPECT_NE(result, 0);
	}

	filesystem::remove_all(folderName);
}


TEST(Files, match_files) {
	using std::string;
	string folderName = joinPaths("sandbox", "match_test");
	string fileName = joinPaths(folderName, "test1.txt");
	string excludeFilename = joinPaths(folderName, "exclude.bin");
	string fileGlobWithEnding = joinPaths(folderName, "*.txt");
	string fileGlobWithoutEnding = joinPaths(folderName, "*");
	string folderToExclude = joinPaths(folderName, "testtfolder");

	try {
		filesystem::remove_all(folderName);
	} catch(filesystem::filesystem_error &) {
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
		EXPECT_EQ(found.size(), 1);
		if (found.size()) {
			EXPECT_EQ(found.front(), fileName);
		}
	}
	{
		auto found = files.findFiles(fileGlobWithEnding);
		EXPECT_EQ(found.size(), 1);
		if (found.size()) {
			EXPECT_EQ(found.front(), fileName);
		}
	}
	{
		auto found = files.findFiles(fileGlobWithoutEnding);

		EXPECT_EQ(found.size(), 2);
	}

	filesystem::remove_all(folderName);
}

TEST(Files, remove_dots) {
	{
		auto result = removeDoubleDots("../..");
		EXPECT_EQ(result, "_/_");
	}

	{
		auto result = removeDoubleDots("..");
		EXPECT_EQ(result, "_");
	}
}

}  // namespace

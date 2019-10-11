#pragma once

#include "mdebug.h"
#include <string>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include "token.h"
#include "merror.h"
#include <array>

#include <stdio.h> //For FILENAME_MAX

#ifdef _WIN32
#include <direct.h>
#define GetCurrentDir _getcwd
#define _popen popen
const string lineSeparator = "\\";
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <unistd.h>

const std::string pathSeparator = "/";

using namespace std;

#include "ifiles.h"

class Files: public IFiles {
public:
    vector<Token> findFiles(Token pattern) override;

    std::pair<int, string> popenWithResult(string command) override;

    time_t getTimeChanged(const std::string &path) override;

	string getCurrentWorkingDirectory() override;

	vector<string> listFiles(string directory) override;

	bool isDirectory(const string &path) override;

	void createDirectory(std::string dir) override;

	string getDirectory(string filename) override;

	std::vector<string> listRecursive(string directory) override;
};

//Creates a directory if it does not exist
void Files::createDirectory(std::string dir) {
    if (system(("mkdir -p " + dir).c_str())) {
        std::runtime_error("could not create directory " + dir);
    }
}

#endif

std::string joinPaths(std::string a, std::string b) {
    return a.empty()? b: (a + pathSeparator + b);
}

// Get the time in seconds when a file is changed. Return 0 if the specified file is not found
time_t Files::getTimeChanged(const std::string &path) {
    struct stat file_stat;
    int err = stat(path.c_str(), &file_stat);
    if (err != 0) {
        // dout << "notice: file does not exist: " << path << endl;
        return 0;
    }
    return file_stat.st_mtime;
}

bool Files::isDirectory(const string &path) {
    struct stat file_stat;
    int err = stat(path.c_str(), &file_stat);
    if (err != 0) {
        // dout << "file or directory " << path << " does not exist" << endl;
        return false;
    }
    return file_stat.st_mode & S_IFDIR;
}



pair<int, string> Files::popenWithResult(string command) {
	pair<int, string> ret;

	FILE *file = popen((command + " 2>&1").c_str(), "r");

	if (!file) {
		return {-1, "failed to execute command " + command};
	}

	constexpr int size = 50;
	char buffer[size];

	while (fgets(buffer, size, file)) {
		ret.second += buffer;
	}

	auto result = pclose(file); // On some systems, not creating a variable for the result leads to a error.
	ret.first = WEXITSTATUS(result);
	return ret;
}


// adapted from https://stackoverflow.com/questions/143174/how-do-i-get-the-directory-that-a-program-is-running-from
string Files::getCurrentWorkingDirectory() {
	std::array<char, FILENAME_MAX> currentPath;
	if (!GetCurrentDir(currentPath.data(), sizeof(currentPath))) {
		throw runtime_error("could not get current working directory");
	}
	return currentPath.data();
}



vector<string> Files::listFiles(string directory) {
	vector<string> ret;

	if (directory.empty()) {
		directory = ".";
	}

	DIR *dir = opendir(directory.c_str());
	struct dirent *ent;

	if (dir) {
		while ((ent = readdir(dir))) {
			string name = ent->d_name;
			if (name != "." && name != "..") {
				ret.emplace_back(name);
			}
		}
		closedir(dir);
	}
	else {
		throw runtime_error("could not open directory " + directory);
	}
	return ret;
}

string Files::getDirectory(string filename) {
	auto directoryFound = filename.rfind("/");
	if (directoryFound != string::npos) {
		return string(filename.begin(), filename.begin() + directoryFound);
	}
	else {
		return "";
	}
}

std::vector<string> Files::listRecursive(string directory) {

	auto list = listFiles(directory);
	auto ret = list;

	for (auto &f: list) {
		if (isDirectory(directory + "/" + f)) {
			auto subList = listRecursive(directory + "/" + f);
			for (auto &s: subList) {
				s = f + "/" + s;
			}
			ret.insert(ret.end(), subList.begin(), subList.end());
		}
	}

	return ret;
}


// trim from both ends (copying)
static inline std::string trim(std::string s) {
	auto front = find_if(s.begin(), s.end(), [] (int ch) {
		return !isspace(ch);
	});
	auto back = find_if(s.rbegin(), s.rend(), [] (int ch) {
					return !isspace(ch);
				}).base();

	return string(front, back);
}


vector<Token> Files::findFiles(Token pattern) {
	pattern = Token(trim(pattern), pattern.location);
	auto found = pattern.find('*');

	vector<Token> ret;
	if (found != string::npos) {
		string beginning(pattern.begin(), pattern.begin() + found);
		string ending;
		string directory = beginning;
		string fileNameBeginning;
		auto directoryEnding = directory.rfind('/');
		if (directoryEnding != string::npos && directoryEnding < found) {
			fileNameBeginning = string(directory.begin() + directoryEnding + 1, directory.begin() + found);
			directory = string(directory.begin(), directory.begin() + directoryEnding);
		}

		vector<string> fileList;
		if (found + 1 < pattern.size() && pattern[found + 1] == '*') {
			// ** means recursive wildcard search
			fileList = listRecursive(directory);
			ending = string(pattern.begin() + found + 2, pattern.end());
		}
		else {
			// * means wildcard search
			fileList = listFiles(directory);
			ending = string(pattern.begin() + found + 1, pattern.end());
		}
		for (auto &file: fileList) {
			auto endingPos = file.find(ending);
			if (endingPos == string::npos) {
				if (ending.empty()) {
					if (file.find(fileNameBeginning) == 0) {
						ret.emplace_back(joinPaths(directory, file), pattern.location);
					}
				}
			}
			else {
				if (endingPos == file.size() - ending.size() &&
					file.find(fileNameBeginning) == 0) {
					ret.emplace_back(joinPaths(directory, file), pattern.location);
				}
			}
		}
	}
	else {
		return {pattern};
	}
	if (ret.empty()) {
		// vout << "warning: pattern " << pattern << " does not match any file" << endl;
	}
//	if (debugOutput && !ret.empty()) {
//		dout << "recursively added:" << endl;
//		for (auto &f: ret) {
//			dout << f << " " << endl;
//		}
//	}
	return ret;
}


static std::pair<Token, Token> stripFileEnding(Token filename, bool allowNoMatch = false) {
	filename = trim(filename);
	if (filename.find(".cpp") == filename.size() - 4) {
		filename = Token(filename.begin(), filename.end() - 4, filename.location);
		return {filename, "cpp"};
	}
	else if (filename.find(".c") == filename.size() - 2) {
		filename = Token(filename.begin(), filename.end() - 2, filename.location);
		return {filename, "c"};
	}
	else if (filename.find(".so") == filename.size() - 3) {
		filename = Token(filename.begin(), filename.end() - 3, filename.location);
		return {filename, "so"};
	}
	else {
		if (allowNoMatch) {
			return {filename, Token("", filename.location)};
		}
		else {
			throw MatmakeError(filename, "unknown filetype in file " + filename);
		}
	}
}


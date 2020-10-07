#pragma once

#include "main/matmake-common.h"
#include "main/mdebug.h"
#include "main/merror.h"
#include "main/token.h"
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include <stdio.h> //For FILENAME_MAX

#ifdef _WIN32
#include <direct.h>
#define GetCurrentDir _getcwd
#define popen _popen
#define pclose _pclose
#include <windows.h>

#ifdef CopyFile
#undef CopyFile
#endif

#else
#include <unistd.h>
#define GetCurrentDir getcwd
#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>
#include <unistd.h>
#endif

#include "environment/ifiles.h"

// Joins two paths and makes sure that the path separator does not
// end up in the beginning of the new path
std::string joinPaths(std::string a, std::string b) {
    if (a.empty()) {
        return b;
    }
    else {
        return a.back() == '/' ? a + b : (a + pathSeparator + b);
    }
}

inline std::pair<Token, Token> stripFileEnding(Token filename,
                                               bool allowNoMatch = false) {
    filename = trim(filename);

    auto matchEnding = [&filename](const std::string &ending) {
        return filename.length() > ending.length() &&
               filename.find(ending) == filename.length() - ending.length();
    };

    if (matchEnding(".cpp")) {
        filename =
            Token(filename.begin(), filename.end() - 4, filename.location);
        return {filename, "cpp"};
    }
    else if (matchEnding(".c")) {
        filename =
            Token(filename.begin(), filename.end() - 2, filename.location);
        return {filename, "c"};
    }
    else if (matchEnding(".so")) {
        filename =
            Token(filename.begin(), filename.end() - 3, filename.location);
        return {filename, "so"};
    }
    else {
        if (allowNoMatch) {
            return {filename, Token("", filename.location)};
        }
        else {
            throw MatmakeError(filename,
                               "unknown filetype in file " + filename);
        }
    }
}

class Files : public IFiles {
public:
    std::vector<Token> findFiles(Token pattern) const override {
        using namespace std;

        pattern = Token(trim(pattern), pattern.location);
        auto found = pattern.find('*');

        std::vector<Token> ret;
        if (found != string::npos) {
            string beginning(pattern.begin(), pattern.begin() + found);
            string ending;
            string directory = beginning;
            string fileNameBeginning;
            auto directoryEnding = directory.rfind('/');
            if (directoryEnding != string::npos && directoryEnding < found) {
                fileNameBeginning =
                    string(directory.begin() + directoryEnding + 1,
                           directory.begin() + found);
                directory = string(directory.begin(),
                                   directory.begin() + directoryEnding);
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
            auto addIfFile = [&ret, this](string filename) {
                if (!isDirectory(filename)) {
                    ret.push_back(filename);
                }
            };
            for (auto &file : fileList) {
                if (ending.empty()) {
                    addIfFile(joinPaths(directory, file));
                }
                else {
                    auto endingPos = file.find(ending);
                    if (endingPos != string::npos) {
                        if (endingPos == file.size() - ending.size() &&
                            file.find(fileNameBeginning) == 0) {
                            addIfFile(joinPaths(directory, file));
                        }
                    }
                }
            }
        }
        else {
            return {pattern};
        }
        return ret;
    }

    std::pair<int, std::string> popenWithResult(
        std::string command) const override {

        std::pair<int, std::string> ret;

        FILE *file = popen((command + " 2>&1").c_str(), "r");

        if (!file) {
            return {-1, "failed to execute command " + command};
        }

        constexpr int size = 50;
        char buffer[size];

        while (fgets(buffer, size, file)) {
            ret.second += buffer;
        }

        auto result = pclose(file); // On some systems, not creating a variable
                                    // for the result leads to a error.
#ifdef _WIN32
        ret.first = result;
#else
        ret.first = WEXITSTATUS(result);
#endif
        return ret;
    }

    time_t getTimeChanged(const std::string &path) const override {
        struct stat file_stat;
        int err = stat(path.c_str(), &file_stat);
        if (err != 0) {
            // dout << "notice: file does not exist: " << path << endl;
            return 0;
        }
        return file_stat.st_mtime;
    }

    // adapted from
    // https://stackoverflow.com/questions/143174/how-do-i-get-the-directory-that-a-program-is-running-from
    std::string getCurrentWorkingDirectory() const override {
        std::array<char, FILENAME_MAX> currentPath;
        if (!GetCurrentDir(currentPath.data(), sizeof(currentPath))) {
            throw std::runtime_error("could not get current working directory");
        }
        return currentPath.data();
    }

    bool setCurrentDirectory(std::string directory) const override {
        return chdir(directory.c_str());
    }

    std::vector<std::string> listFiles(std::string directory) const override {
        using namespace std;

        vector<string> ret;

        if (directory.empty()) {
            directory = ".";
        }

#ifdef _WIN32
        WIN32_FIND_DATA findData;
        HANDLE fileHandle = FindFirstFileA(directory.c_str(), &findData);
        if (fileHandle != INVALID_HANDLE_VALUE) {
            while (FindNextFile(fileHandle, &findData)) {
                string name = findData.cFileName;

                if (name != "." && name != "..") {
                    ret.emplace_back(name);
                }
            }
            FindClose(fileHandle);
        }
        return ret;
#else
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
#endif
    }

    bool isDirectory(const std::string &path) const override {
        struct stat file_stat;
        int err = stat(path.c_str(), &file_stat);
        if (err != 0) {
            // dout << "file or directory " << path << " does not exist" <<
            // endl;
            return false;
        }
        return file_stat.st_mode & S_IFDIR;
    }

    // Creates a directory if it does not exist
    void createDirectory(std::string dir) const override {
        if (system(("mkdir -p " + dir).c_str())) {
            std::runtime_error("could not create directory " + dir);
        }
    }

    std::string getDirectory(std::string filename) const override {
        auto directoryFound = filename.rfind("/");
        if (directoryFound != std::string::npos) {
            return std::string(filename.begin(),
                               filename.begin() + directoryFound);
        }
        else {
            return "";
        }
    }

    std::vector<std::string> listRecursive(
        std::string directory) const override {

        auto list = listFiles(directory);
        auto ret = list;

        for (auto &f : list) {
            if (isDirectory(directory + "/" + f)) {
                auto subList = listRecursive(directory + "/" + f);
                for (auto &s : subList) {
                    s = f + "/" + s;
                }
                ret.insert(ret.end(), subList.begin(), subList.end());
            }
        }

        return ret;
    }

    int remove(std::string filename) const override {
        return ::remove(filename.c_str());
    }
};

std::string removeDoubleDots(std::string str) {
    auto findStr = ".." + pathSeparator;
    auto replaceStr = "_" + pathSeparator;

    for (auto found = str.find(findStr); found != std::string::npos;
         found = str.find(findStr)) {

        str.replace(found, findStr.length(), replaceStr);

        found += replaceStr.length();
    }

    if (str.length() == 1) {
        return str;
    }
    else if (str.length() == 2 && str == "..") {
        str = "_";
    }
    else if (str.substr(str.length() - 2, 2) == "..") {
        str.replace(str.length() - 2, 2, "_");
    }

    return str;
}

#pragma once

#include "environment/ifiles.h"
#include "environment/popenstream.h"
#include "main/matmake-common.h"
#include "main/mdebug.h"
#include "main/merror.h"
#include "main/token.h"
#include <array>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

//#if defined(_WIN32) or defined(__MINGW32__)
//#define popen _popen
//#define pclose _pclose
//#include <windows.h>
//#endif

//#include <cstdio> //For FILENAME_MAX

//#ifdef _WIN32
//#include <direct.h>
//#define GetCurrentDir _getcwd
//#define popen _popen
//#define pclose _pclose
//#include <windows.h>

#ifdef CopyFile
#undef CopyFile
#endif

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

//#else
//#include <unistd.h>
//#define GetCurrentDir getcwd
//#include <sys/stat.h>
//#include <sys/types.h>

//#include <dirent.h>
//#include <unistd.h>
//#endif

#if __has_include(<filesystem>)

#include <filesystem>
namespace filesystem = std::filesystem;

#else

#include <experimental/filesystem>
namespace filesystem = std::experimental::filesystem;

#endif

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

inline std::string getDirectory(std::string path) {
    auto f = path.rfind('/');
    if (f != std::string::npos) {
        return path.substr(0, f);
    }
    else {
        return path;
    };
}

inline std::string getFilename(std::string path, std::string whenNoDir = ".") {
    if (whenNoDir.empty()) {

        return filesystem::path{path}.filename().string();
    }
    else {
        auto f = filesystem::path{path}.filename().string();
        if (f.empty()) {
            return whenNoDir;
        }
        else {
            return f;
        }
    }
    //    auto f = path.rfind('/');
    //    if (f != std::string::npos) {
    //        return path.substr(f + 1);
    //    }
    //    else {
    //        return whenNoDir;
    //    };
}

inline std::pair<Token, Token> stripFileEnding(Token filename,
                                               bool allowNoMatch = false) {
    filename = trim(filename);

    auto matchEnding = [&filename](const std::string &ending) {
        return filename.length() > ending.length() &&
               filename.find(ending) == filename.length() - ending.length();
    };

    // Ze best way to strip file endings right? ^^
    if (matchEnding(".cpp")) {
        filename =
            Token(filename.begin(), filename.end() - 4, filename.location);
        return {filename, "cpp"};
    }
    else if (matchEnding(".cc")) {
        filename =
            Token(filename.begin(), filename.end() - 3, filename.location);
        return {filename, "cpp"};
    }
    else if (matchEnding(".cppm")) {
        filename =
            Token(filename.begin(), filename.end() - 5, filename.location);
        return {filename, "cppm"};
    }
    else if (matchEnding(".pcm")) {
        filename =
            Token(filename.begin(), filename.end() - 4, filename.location);
        return {filename, "pcm"};
    }
    else if (matchEnding(".c")) {
        filename =
            Token(filename.begin(), filename.end() - 2, filename.location);
        return {filename, "c"};
    }
    else if (matchEnding(".o")) {
        filename =
            Token(filename.begin(), filename.end() - 2, filename.location);
        return {filename, "o"};
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
                            addIfFile(file);
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

    int system(const std::string &command) const override {
        return std::system(command.c_str());
    }

    //! It might not return real time t, but it is a usable value so whatever
    time_t getTimeChanged(const std::string &path) const override {
        if (!filesystem::exists(path)) {
            return 0;
        }
        else {
            auto t = filesystem::last_write_time(path);
            return t.time_since_epoch().count();
        }
    }

    std::ifstream openRead(const std::string &path) const override {
        std::ifstream file(path);
        if (!file.is_open()) {
            file.get(); // Trigger error if not open
            file.unget();
        }
        return file;
    }

    std::string currentDirectory() const override {
        return filesystem::current_path().string();
    }

    bool currentDirectory(std::string directory) const override {
        std::error_code ec;
        filesystem::current_path(directory, ec);
        return ec.value();
    }

    std::vector<std::string> listFiles(std::string directory) const override {
        using namespace std;

        vector<string> ret;

        if (directory.empty()) {
            directory = ".";
        }

        for (auto it : filesystem::directory_iterator{directory}) {
            ret.push_back(it.path().string());
        }

        return ret;
    }

    bool isDirectory(const std::string &path) const override {
        return filesystem::is_directory(path);
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

        auto ret = std::vector<std::string>{};
        ret.reserve(100);

        for (auto it : filesystem::recursive_directory_iterator{directory}) {
            ret.push_back(it.path().string());
        }

        return ret;
    }

    int remove(std::string filename) const override {
        return ::remove(filename.c_str());
    }

    void appendToFile(std::string name, std::string value) const override {
        std::ofstream(name, std::ofstream::app) << value;
    }

    void replaceFile(std::string name, std::string value) const override {
        std::ofstream(name) << value;
    }

    void copyFile(std::string source, std::string destination) const override {
        std::ifstream src(source);
        if (!src.is_open()) {
            throw std::runtime_error("could not open input file " + source +
                                     " for copy for target ");
        }

        std::ofstream dst(destination);
        if (!dst) {
            throw std::runtime_error("could not open file " + destination +
                                     +" for output");
        }

        dst << src.rdbuf();
    }

    std::vector<std::string> readLines(std::string source) const override {
        std::ifstream file(source);
        if (!file.is_open()) {
            throw std::runtime_error("could not open file " + source);
        }
        std::vector<std::string> lines;

        for (std::string line; getline(file, line);) {
            lines.emplace_back(move(line));
        }

        return lines;
    };

    std::pair<std::vector<std::string>, std::string> parseDepFile(
        Token depFile) const override {
        using namespace std;
        try {
            auto lines = readLines(depFile);
            vector<string> ret;
            string command;

            bool firstLine = true;
            for (auto line : lines) {
                istringstream ss(line);

                if (!line.empty() && line.front() == '\t') {
                    command = trim(line);
                    break;
                }
                else {
                    string d;
                    if (firstLine) {
                        ss >> d; // The first is the target path --> ignore
                        firstLine = false;
                    }
                    while (ss >> d) {
                        if (d !=
                            "\\") { // Skip backslash (is used before newlines)
                            ret.push_back(d);
                        }
                    }
                }
            }
            return {ret, command};
        }
        catch (std::runtime_error &e) {
            dout << "could not find .d file " << depFile << endl;
            return {};
        }
    }
};

std::string removeDoubleDots(std::string str) {
    if (str.empty()) {
        return {};
    }

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

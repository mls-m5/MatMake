#pragma once

#include <vector>
#include "token.h"

class IFiles {
public:
    virtual ~IFiles() = default;

    virtual std::vector<Token> findFiles(Token pattern) = 0;

    virtual std::pair<int, std::string> popenWithResult(std::string command) = 0;

    virtual time_t getTimeChanged(const std::string &path) = 0;

    virtual std::string getCurrentWorkingDirectory() = 0;

    virtual std::vector<std::string> listFiles(std::string directory) = 0;

    virtual bool isDirectory(const std::string &path) = 0;

    virtual void createDirectory(std::string dir) = 0;

    virtual std::string getDirectory(std::string filename) = 0;

    virtual std::vector<std::string> listRecursive(std::string directory) = 0;

    virtual std::string removeDoubleDots(std::string string) = 0;
};

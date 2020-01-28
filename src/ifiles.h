#pragma once

#include <vector>
#include "token.h"

class IFiles {
public:
    virtual ~IFiles() = default;

    virtual std::vector<Token> findFiles(Token pattern) const = 0;

    virtual std::pair<int, std::string> popenWithResult(std::string command) const = 0;

    virtual time_t getTimeChanged(const std::string &path) const = 0;

    virtual std::string getCurrentWorkingDirectory() const = 0;

    virtual bool setCurrentDirectory(std::string directory) const = 0;

    virtual std::vector<std::string> listFiles(std::string directory) const = 0;

    virtual bool isDirectory(const std::string &path) const = 0;

    virtual void createDirectory(std::string dir) const = 0;

    virtual std::string getDirectory(std::string filename) const = 0;

    virtual std::vector<std::string> listRecursive(std::string directory) const = 0;

    virtual std::string removeDoubleDots(std::string string) const = 0;
};

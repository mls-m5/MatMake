#pragma once

#include "main/token.h"
#include <vector>

class IFiles {
public:
    virtual ~IFiles() = default;

    virtual std::vector<Token> findFiles(Token pattern) const = 0;

    virtual std::pair<int, std::string> popenWithResult(
        std::string command) const = 0;

    //! Return timecode when time is changed, lower is older, higher is newer
    //! return 0 if file is not found
    virtual time_t getTimeChanged(const std::string &path) const = 0;

    virtual std::string getCurrentWorkingDirectory() const = 0;

    virtual bool setCurrentDirectory(std::string directory) const = 0;

    virtual std::vector<std::string> listFiles(std::string directory) const = 0;

    virtual bool isDirectory(const std::string &path) const = 0;

    virtual void createDirectory(std::string dir) const = 0;

    virtual std::string getDirectory(std::string filename) const = 0;

    virtual std::vector<std::string> listRecursive(
        std::string directory) const = 0;

    virtual int remove(std::string filename) const = 0;

    virtual void replaceFile(std::string name, std::string value) const = 0;
    virtual void appendToFile(std::string name, std::string value) const = 0;

    //! @throws std::runtime_exception on fail
    virtual void copyFile(std::string source,
                          std::string destination) const = 0;

    virtual std::vector<std::string> readLines(std::string source) const = 0;

    virtual std::pair<std::vector<std::string>, std::string> parseDepFile(
        Token depFile) const = 0;
};

std::string removeDoubleDots(std::string string);

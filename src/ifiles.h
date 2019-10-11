#pragma once

#include <vector>
#include "token.h"

class IFiles {
public:
    virtual ~IFiles() = default;

    virtual std::vector<Token> findFiles(Token pattern) = 0;

    virtual std::pair<int, std::string> popenWithResult(std::string command) = 0;

    virtual time_t getTimeChanged(const std::string &path) = 0;
};

// Copyright Mattias Larsson Sköld 2019
#pragma once

#include "token.h"

//! Describes the full name of a variable,
//! ie target.propertyName
struct NameDescriptor {
    NameDescriptor(std::vector<Token> name) {
        // Join names combined with for example "-"
        while (name.size() > 1 && name.front().trailingSpace.empty() &&
               name[1] != "." && name[1] != "=" && name[1] != "+=") {
            name.front() += name[1];
            name.front().trailingSpace = name[1].trailingSpace;
            name.erase(name.begin() + 1);
        }

        if (name.size() == 1) {
            propertyName = name.front();
        }
        else if (name.size() == 3 && name[1] == ".") {
            rootName = name.front();
            propertyName = name.back();
        }
    }

    NameDescriptor(const Token &name, const Token &propertyName)
        : rootName(name), propertyName(propertyName) {}

    bool empty() {
        return propertyName.empty();
    }

    Token rootName = "root";
    Token propertyName = "";
};

#ifdef _WIN32

const std::string pathSeparator = "\\";
#else

const std::string pathSeparator = "/";
#endif

//! Reference to main function
int start(std::vector<std::string> args);

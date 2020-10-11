#pragma once

#include "main/token.h"

#include <map>

class IDependency;
class Globals;

class IEnvironment {
public:
    virtual ~IEnvironment() = default;

    //! Add references to other folders with matmake or makefiles in them
    virtual void addExternalDependency(bool shouldCompileBefore,
                                       const Token &name,
                                       const Tokens &args) = 0;

    virtual void compile(std::vector<std::string> targetArguments) = 0;
    virtual void clean(std::vector<std::string> targetArguments) = 0;
    virtual void listAlternatives() const = 0;
};

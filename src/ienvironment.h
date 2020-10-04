#pragma once

#include "token.h"

#include <map>

class IDependency;
class Globals;

class IEnvironment {
public:
    virtual ~IEnvironment() = default;

    //	virtual class IBuildTarget *findTarget(Token name) const = 0;

    //	virtual void addTask(IDependency *t) = 0;
    //	virtual void addTaskCount() = 0;

    //! Add references to other folders with matmake or makefiles in them
    virtual void addExternalDependency(bool shouldCompileBefore,
                                       const Token &name,
                                       const Tokens &args) = 0;

    virtual void appendVariable(const class NameDescriptor &name,
                                Tokens value) = 0;

    virtual void setVariable(const class NameDescriptor &name,
                             Tokens value) = 0;

    //! Add variables to root target to be inherited from
    virtual void setCommandLineVars(
        const std::map<std::string, std::vector<std::string>> &vars) = 0;

    virtual void compile(std::vector<std::string> targetArguments) = 0;
    virtual void clean(std::vector<std::string> targetArguments) = 0;
    virtual void listAlternatives() const = 0;

    //	virtual int getBuildProgress() const = 0;
};

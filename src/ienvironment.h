#pragma once

#include "token.h"

class IDependency;
class Globals;

class IEnvironment {
public:
	virtual ~IEnvironment() = default;

	virtual class IFiles &fileHandler() = 0;

	virtual class IBuildTarget *findTarget(Token name) = 0;

	virtual void addTask(IDependency *t, bool count) = 0;
	virtual void addTaskCount() = 0;

	virtual Globals &globals() = 0;

	virtual void addExternalDependency(bool shouldCompileBefore,
									   const Token &name,
									   const Tokens &args) = 0;

	virtual void appendVariable(
		const class NameDescriptor &name,
		Tokens value) = 0;

	virtual void setVariable(
		const class NameDescriptor &name,
		Tokens value) = 0;

	virtual void compile(std::vector<std::string> targetArguments) = 0;
	virtual void clean(std::vector<std::string> targetArguments) = 0;
	virtual void listAlternatives() = 0;
};

#pragma once

#include "token.h"

class IDependency;
class Globals;

class IEnvironment {
public:
	virtual ~IEnvironment() = default;

	virtual const class IFiles &fileHandler() const = 0;

	virtual class IBuildTarget *findTarget(Token name) = 0;

	virtual void addTask(IDependency *t, bool count) = 0;
	virtual void addTaskCount() = 0;

	virtual const Globals &globals() const = 0;

	//! Add references to other folders with matmake or makefiles in them
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

	virtual int getBuildProgress() = 0;
};

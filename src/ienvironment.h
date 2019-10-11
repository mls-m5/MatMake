#pragma once

#include "token.h"

class IDependency;

class IEnvironment {
public:
	virtual ~IEnvironment() = default;

	virtual class IFiles &fileHandler() = 0;

	virtual class IBuildTarget *findTarget(Token name) = 0;


	virtual void addTask(IDependency *t, bool count) = 0;
	virtual void addTaskCount() = 0;
};

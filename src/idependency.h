#pragma once

#include "token.h"

class IDependency {
public:
	virtual ~IDependency() = default;
	virtual class IEnvironment &env() = 0;

	virtual void dirty(bool) = 0;
	virtual bool dirty() = 0;
	virtual void clean() = 0;
	virtual time_t build() = 0;
	virtual void work() = 0;

	//! Add task to list of tasks to be executed
	//! the count variable sepcify if the dependency should be counted (true)
	//! if hintStatistic has already been called
	virtual void queue(bool count) = 0;

	//! Tell the environment that there will be a dependency added in the future
	//! This is when the dependency is waiting for other files to be made
	virtual void hintStatistic() = 0;


	virtual void notice(IDependency * d) = 0;

	virtual Token targetPath() = 0;
};

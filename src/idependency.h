#pragma once

#include "token.h"
#include "buildtype.h"

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

	//! Tell a dependency that the built of this file is finished
	virtual void notice(IDependency * d) = 0;

	//! When the targets dependency is fresh and the target is ready to be built
	virtual void dependenciesComplete() = 0;

	//! The path to where the target will be built
	virtual Token targetPath() = 0;

	//! A subscriber is a dependency that want a notice when the file is built
	virtual void addSubscriber(IDependency *s) = 0;

	//! Add a file that this file will wait for
	virtual void addDependency(IDependency *file) = 0;

	virtual bool includeInBinary() = 0;

	virtual BuildType buildType() = 0;

	virtual Token linkString() = 0;
};

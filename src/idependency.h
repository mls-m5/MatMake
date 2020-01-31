#pragma once

#include "token.h"
#include "buildtype.h"
#include <set>

class IDependency {
public:
	virtual ~IDependency() = default;
	virtual class IEnvironment &env() = 0;
	virtual const class IEnvironment &env() const = 0;

	virtual void dirty(bool) = 0;
	virtual bool dirty() const = 0;
	virtual void clean() = 0;
	virtual void build() = 0;
	virtual void work() = 0;

	//! Get the time when the file was latest changed
	//! 0 means that the file is not built at all
	virtual time_t changedTime() const = 0;

	//! Tell a dependency that the built of this file is finished
	//! Set pruned to true if
	virtual void notice(IDependency * d, bool pruned = false) = 0;

	//! The path to where the target will be built
	virtual Token output() const = 0;

	//! The main target and implicit targets (often dependency files)
	virtual const vector<Token>& outputs() const = 0;

	//! A subscriber is a dependency that want a notice when the file is built
	virtual void addSubscriber(IDependency *s) = 0;

	//! Add a file that this file will wait for
	virtual void addDependency(IDependency *file) = 0;

	//! Remove fresh dependencies
	virtual void prune() = 0;

	virtual bool includeInBinary() const = 0;

	virtual BuildType buildType() const = 0;

	virtual Token linkString() const = 0;

	virtual const std::set<class IDependency*> dependencies() const = 0;

	//! Create ninja specification for this file
	//! this is not yet implemented
	virtual std::string createNinjaDescription() const = 0;

	virtual const IBuildTarget* target() const = 0;
};

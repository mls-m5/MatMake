// Copyright Mattias Larsson Sköld

#pragma once

#include <chrono>
#include "ienvironment.h"
#include "idependency.h"
#include "matmake-common.h"
#include "files.h"
#include <mutex>
#include <set>

class Dependency: public IDependency {
public:
	Dependency(IEnvironment *env): _env(env) {}

	virtual ~Dependency() override = default;

	virtual time_t getTimeChanged() {
		return env().fileHandler().getTimeChanged(targetPath());
	}

	virtual time_t build() override = 0;
	virtual void work() override = 0;

	void addDependency(IDependency *file) override {
		if (file) {
			_dependencies.insert(file);
		}
	}

	//! Add task to list of tasks to be executed
	//! the count variable sepcify if the dependency should be counted (true)
	//! if hintStatistic has already been called
	void queue(bool count) override {
		_env->addTask(this, count);
	}

	//! Tell the environment that there will be a dependency added in the future
	//! This is when the dependency is waiting for other files to be made
	void hintStatistic() override {
		_env->addTaskCount();
	}

	Token targetPath() override = 0;

	void clean() override = 0;

	bool includeInBinary() override { return true; }

	void addSubscriber(IDependency *s) override {
		lock_guard<mutex> guard(accessMutex);
		if (find(_subscribers.begin(), _subscribers.end(), s) == _subscribers.end()) {
			_subscribers.push_back(s);
		}
	}

	//! Send a notice to all subscribers
	virtual void sendSubscribersNotice() {
		lock_guard<mutex> guard(accessMutex);
		for (auto s: _subscribers) {
			s->notice(this);
		}
		_subscribers.clear();
	}

	virtual class IEnvironment &env() override {
		return *_env;
	}

	//! A message from a object being subscribed to
	//! This is used by targets to know when all dependencies
	//! is built
	void notice(IDependency *) override {}

	void lock() {
		accessMutex.lock();
	}

	void unlock() {
		accessMutex.unlock();
	}

	bool dirty() override { return _dirty; }
	void dirty(bool value) override { _dirty = value; }

	const set<class IDependency*> dependencies() const { return _dependencies; }
	const vector <IDependency*> & subscribers() const { return _subscribers; }

	IEnvironment *environment() { return _env; }


private:
	IEnvironment *_env;
	set<class IDependency*> _dependencies;
	vector <IDependency*> _subscribers;
	mutex accessMutex;
	bool _dirty = false;
};

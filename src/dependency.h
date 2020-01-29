// Copyright Mattias Larsson Sköld

#pragma once

#include <chrono>
#include "ienvironment.h"
#include "idependency.h"
#include "matmake-common.h"
#include "globals.h"
#include "files.h"
#include <mutex>
#include <set>

class Dependency: public IDependency {
	IEnvironment *_env;
	set<class IDependency*> _dependencies;
	vector <IDependency*> _subscribers;
	mutex accessMutex;
	bool _dirty = false;

	vector <Token> _outputs;
	vector <Token> _inputs;
	Token _command;
	Token _depFile;

public:
	Dependency(IEnvironment *env): _env(env) {}

	virtual ~Dependency() override = default;

	virtual time_t getTimeChanged() {
		return env().fileHandler().getTimeChanged(output());
	}

	virtual void build() override = 0;
	virtual void work() override = 0;

	//! Returns the changed time of the oldest of all output files
	virtual time_t getChangedTime() const override {
		time_t outputChangedTime = std::numeric_limits<time_t>::max();

		for (auto &out: _outputs) {
			auto changedTime = env().fileHandler().getTimeChanged(out);

			outputChangedTime = std::min(outputChangedTime, changedTime);
		}

		return outputChangedTime;
	}

	void addDependency(IDependency *file) override {
		if (file) {
			_dependencies.insert(file);
		}
	}

	//! Add task to list of tasks to be executed
	//! if hintStatistic has already been called called is set to false to not
	//! count the dependency twice
	void queue(bool count) override {
		_env->addTask(this, count);
	}

	//! Tell the environment that there will be a dependency added in the future
	//! This is when the dependency is waiting for other files to be made
	void hintStatistic() override {
		_env->addTaskCount();
	}

	Token output() const final {
		if (_outputs.empty()) {
			return "";
		}
		else {
			return _outputs.front();
		}
	}

	void clean() override {
		vout << "removing file " << output() << endl;
		for (auto &out: outputs()) {
			vout << "removing file " << out << endl;
			remove(out.c_str());
		}
	}

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

	virtual const class IEnvironment &env() const override {
		return *_env;
	}

	//! A message from a object being subscribed to
	//! This is used by targets to know when all dependencies
	//! is built
	void notice(IDependency *d) override {
		_dependencies.erase(d);
		if (_dependencies.empty()) {
			dependenciesComplete();
		}
	}

	void dependenciesComplete() override {
		queue(true);
	}

	void lock() {
		accessMutex.lock();
	}

	void unlock() {
		accessMutex.unlock();
	}

	bool dirty() const override { return _dirty; }
	void dirty(bool value) override { _dirty = value; }

	const set<class IDependency*> dependencies() const override {
		return _dependencies;
	}
	const vector <IDependency*> & subscribers() const { return _subscribers; }

	IEnvironment *environment() { return _env; }

	const Token &command() const {
		return _command;
	}

	void command(Token command) {
		_command = std::move(command);
	}

	//! Set the primary output file for the dependency
	void output(Token output) {
		_outputs.insert(_outputs.begin(), output);
	}

	// Set which file to search for dependencies in
	// this file is also a implicit output file
	void depFile(Token file) {
		_depFile = file;
		_outputs.push_back(file);
	}

	Token depFile() const {
		return _depFile;
	}

	const vector<Token>& outputs() const {
		return _outputs;
	}

	void inputs(vector<Token> in) {
		_inputs = std::move(in);
	}

	std::string createNinjaDescription() {
		std::string outputsString, inputsString;
		for (auto &out: _outputs) {
			outputsString += (" " + out);
		}
		for (auto &in: _inputs) {
			inputsString += (" " + in);
		}
		return "build" + outputsString + ":???_??? " + inputsString;
	}


};

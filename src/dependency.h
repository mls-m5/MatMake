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
	set<class IDependency*> _dependencies; // Dependencies from matmakefile
	set<IDependency*> _subscribers;
	mutex _accessMutex;
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

	virtual void work() override {
		if (!command().empty()) {
			vout << command() << endl;
			pair <int, string> res = env().fileHandler().popenWithResult(command());
			if (res.first) {
				throw MatmakeError(command(), "could not build object:\n" + command() + "\n" + res.second);
			}
			else if (!res.second.empty()) {
				cout << (command() + "\n" + res.second + "\n") << std::flush;
			}
			dirty(false);
			sendSubscribersNotice();
		}
	}

	//! Returns the changed time of the oldest of all output files
	virtual time_t changedTime() const override {
		time_t outputChangedTime = std::numeric_limits<time_t>::max();

		for (auto &out: _outputs) {
			auto changedTime = env().fileHandler().getTimeChanged(out);

			outputChangedTime = std::min(outputChangedTime, changedTime);
		}

		return outputChangedTime;
	}

	void addDependency(IDependency *file) override {
		if (file) {
			dout << "adding " << file->output() << " to " << output() << endl;
			_dependencies.insert(file);
		}
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
		for (auto &out: outputs()) {
			vout << "removing file " << out << endl;
			remove(out.c_str());
		}
	}

	bool includeInBinary() override { return true; }

	void addSubscriber(IDependency *s) override {
		lock_guard<mutex> guard(_accessMutex);
		if (find(_subscribers.begin(), _subscribers.end(), s) == _subscribers.end()) {
			_subscribers.insert(s);
		}
	}

	//! Send a notice to all subscribers
	virtual void sendSubscribersNotice(bool pruned = false) {
		lock_guard<mutex> guard(_accessMutex);
		for (auto s: _subscribers) {
			s->notice(this, pruned);
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
	void notice(IDependency *d, bool pruned = false) override {
		_dependencies.erase(d);
		if (env().globals().debugOutput) {
			dout << "removing dependency " << d->output() << " from " << output() << endl;
			dout << "   " << _dependencies.size() << " remains ";
			for (auto d: _dependencies) {
				dout << d->output() << " " << endl;
			}
		}
		if (_dependencies.empty()) {
			if (!pruned) {
				env().addTask(this);
				dout << "Adding " << output() << " to task list " << endl;
			}
		}
	}

	void lock() {
		_accessMutex.lock();
	}

	void unlock() {
		_accessMutex.unlock();
	}

	bool dirty() const override { return _dirty; }
	void dirty(bool value) override { _dirty = value; }

	const set<class IDependency*> dependencies() const override {
		return _dependencies;
	}

	const set <IDependency*> & subscribers() const { return _subscribers; }

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

	void prune() override {
		vector<IDependency*> toRemove;
		for (auto* dep: _dependencies) {
			if (!dep->dirty()) {
				toRemove.push_back(dep);
			}
		}
		for (auto d: toRemove) {
			_dependencies.erase(d);
		}
	}
};

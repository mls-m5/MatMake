// Copyright Mattias Larsson Sköld

#pragma once

#include <chrono>
#include "idependency.h"
#include "matmake-common.h"
#include "threadpool.h"
#include "globals.h"
#include "files.h"
#include <mutex>
#include <set>

class IBuildTarget;

class Dependency: public IDependency {
	set<class IDependency*> _dependencies; // Dependencies from matmakefile
	set<IDependency*> _subscribers;
	mutex _accessMutex;
	bool _dirty = false;

	vector <Token> _outputs;
	vector <Token> _inputs;
	Token _command;
	Token _depFile;
	IBuildTarget* _target;

public:
	Dependency(IBuildTarget *target) :
			_target(target) {
	}

	virtual ~Dependency() override = default;

	// Get the latest time of times changed for input files
	virtual time_t inputChangedTime(const IFiles &files) const {
		time_t time = 0;

		for (auto &input: inputs()) {
			time = std::max(time, files.getTimeChanged(input));
		}

		return time;
	}


	//! Returns the changed time of the oldest of all output files
	virtual time_t changedTime(const IFiles &files) const override {
		time_t outputChangedTime = std::numeric_limits<time_t>::max();

		for (auto &out: _outputs) {
			auto changedTime = files.getTimeChanged(out);

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

	void clean(const IFiles &files) override {
		for (auto &out: outputs()) {
			if (std::find(_inputs.begin(), _inputs.end(), out) != _inputs.end()) {
				continue; // Do not remove source files
			}
			vout << "removing file " << out << endl;
			files.remove(out.c_str());
		}
	}

	bool includeInBinary() const override { return true; }

	void addSubscriber(IDependency *s) override {
		lock_guard<mutex> guard(_accessMutex);
		if (find(_subscribers.begin(), _subscribers.end(), s) == _subscribers.end()) {
			_subscribers.insert(s);
		}
	}

	//! Send a notice to all subscribers
	virtual void sendSubscribersNotice(ThreadPool& pool) {
		lock_guard<mutex> guard(_accessMutex);
		for (auto s: _subscribers) {
			s->notice(this, pool);
		}
		_subscribers.clear();
	}

	//! A message from a object being subscribed to
	//! This is used by targets to know when all dependencies
	//! is built
	void notice(IDependency *d, ThreadPool& pool) override {
		_dependencies.erase(d);
		if (globals.debugOutput) {
			dout << "removing dependency " << d->output() << " from " << output() << endl;
			dout << "   " << _dependencies.size() << " remains ";
			for (auto d: _dependencies) {
				dout << d->output() << " " << endl;
			}
		}
		if (_dependencies.empty()) {
			pool.addTask(this);
			dout << "Adding " << output() << " to task list " << endl;
		}
	}

	void lock() {
		_accessMutex.lock();
	}

	void unlock() {
		_accessMutex.unlock();
	}

	bool dirty() const final { return _dirty; }
	void dirty(bool value) final { _dirty = value; }

	const set<class IDependency*> dependencies() const override {
		return _dependencies;
	}

	const set <IDependency*> & subscribers() const { return _subscribers; }

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

	void input(Token in) {
		_inputs = {std::move(in)};
	}

	vector<Token> inputs() const {
		return _inputs;
	}

	Token input() const {
		if (_inputs.empty()) {
			return "";
		}
		else {
			return _inputs.front();
		}
	}

	Token linkString() const override {
		return output();
	}

	IBuildTarget *target() const override {
		return _target;
	}

	virtual void work(const IFiles &files, ThreadPool& pool) override {
		if (!command().empty()) {
			vout << command() << endl;
			pair <int, string> res = files.popenWithResult(command());
			if (res.first) {
				throw MatmakeError(command(), "could not build object:\n" + command() + "\n" + res.second);
			}
			else if (!res.second.empty()) {
				cout << (command() + "\n" + res.second + "\n") << std::flush;
			}
			dirty(false);
			sendSubscribersNotice(pool);
		}
	}

	void prune() override {
		dout << "pruning " << output() << endl;
		vector<IDependency*> toRemove;
		for (auto* dep: _dependencies) {
			if (!dep->dirty()) {
				toRemove.push_back(dep);
			}
		}
		for (auto d: toRemove) {
			dout << "removing dependency " << d->output() << endl;
			_dependencies.erase(d);
		}
	}
};

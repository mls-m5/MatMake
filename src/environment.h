// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "ienvironment.h"

#include "globals.h"
#include "token.h"
#include "ibuildtarget.h"
#include "targets.h"
#include <queue>
#include <atomic>

#include <fstream>

class Environment: public IEnvironment {
public:
	struct ExternalMatmakeType {
		bool compileBefore;
		Token name;
		Tokens arguments;
	};

	Targets targets;
	queue<IDependency *> tasks;
	mutex workMutex;
	mutex workAssignMutex;
	std::atomic<size_t> numberOfActiveThreads;
	std::shared_ptr<IFiles> _fileHandler;
	int maxTasks = 0;
	int taskFinished = 0;
	int lastProgress = 0;
	bool finished = false;

	std::vector<ExternalMatmakeType> externalDependencies;

	void addExternalDependency(bool shouldCompileBefore,
							   const Token &name,
							   const Tokens &args) override {
		externalDependencies.push_back({shouldCompileBefore,
										name,
										args});
	}

	void buildExternal(bool isBefore, string operation) {
		for (auto &dependency: externalDependencies) {
			if (dependency.compileBefore != isBefore) {
				continue;
			}

			auto currentDirectory = _fileHandler->getCurrentWorkingDirectory();

			if (!_fileHandler->setCurrentDirectory(dependency.name)) {
				vector <string> newArgs(dependency.arguments.begin(),
									   dependency.arguments.end());

				if (operation == "clean") {
					cout << endl << "cleaning external " << dependency.name << endl;
					start({"clean"}, globals);
				}
				else {
					cout << endl << "Building external " << dependency.name << endl;
					start(newArgs, globals);
					if (globals.bailout) {
						break;
					}
					cout << endl;
				}
			}
			else {
				throw runtime_error("could not external directory " + dependency.name);
			}

			if (_fileHandler->setCurrentDirectory(currentDirectory)) {
				throw runtime_error("could not go back to original working directory " + currentDirectory);
			}
			else {
				vout << "returning to " << currentDirectory << endl;
				vout << endl;
			}
		}
	}

	void addTask(IDependency *t) override {
		workAssignMutex.lock();
		tasks.push(t);
		workAssignMutex.unlock();
		workMutex.try_lock();
		workMutex.unlock();
	}

	void addTaskCount() override {
		++maxTasks;
	}

	Environment (shared_ptr<IFiles> fileHandler):
		  _fileHandler(fileHandler)
	{
		targets.emplace_back(new BuildTarget("root"));
	}


	IBuildTarget *findVariable(Token name) const {
		if (name.empty()) {
			return nullptr;
		}
		for (auto &v: targets) {
			if (v->name() == name) {
				return v.get();
			}
		}
		return nullptr;
	}

	//! Gets a property from a target name plus property name
	Tokens getValue(NameDescriptor name) const {
		if (auto variable = findVariable(name.rootName)) {
			return variable->get(name.propertyName);
		}
		else {
			return {};
		}
	}

	IBuildTarget &operator[] (Token name) {
		if (auto v = findVariable(name)) {
			return *v;
		}
		else {
			targets.emplace_back(new BuildTarget(name));
			return *targets.back();
		}
	}

	void appendVariable(const NameDescriptor &name, Tokens value) override {
		(*this) [name.rootName].append(name.propertyName, value);
	}

	void setVariable(const NameDescriptor &name, Tokens value) override {
		(*this) [name.rootName].assign(name.propertyName, value, targets) ;
	}

	void print() {
		for (auto &v: targets) {
			v->print();
		}
	}

	int getBuildProgress() const override {
		return (taskFinished * 100 / maxTasks);
	}

	void printProgress() {
		if (!maxTasks) {
			return;
		}
		int amount = getBuildProgress();

		if (amount == lastProgress) {
			return;
		}
		lastProgress = amount;

		stringstream ss;

		if (!globals.debugOutput && !globals.verbose) {
			ss << "[";
			for (int i = 0; i < amount / 4; ++i) {
				ss << "-";
			}
			if (amount < 100) {
				ss << ">";
			}
			else {
				ss << "-";
			}
			for (int i = amount / 4; i < 100 / 4; ++i) {
				ss << " ";
			}
			ss << "] " << amount << "%  \r";
		}
		else {
			//ss << "[" << amount << "%] ";
		}

		cout << ss.str();
		cout.flush();
	}

	vector<IBuildTarget*> parseTargetArguments(vector<string> targetArguments) const {
		vector<IBuildTarget*> selectedTargets;

		if (targetArguments.empty()) {
			for (auto& target: targets) {
				selectedTargets.push_back(target.get());
			}
		}
		else {
			for (auto t: targetArguments) {
				bool matchFailed = true;
				auto target = targets.find(t);
				if (target) {

					selectedTargets.push_back(target);
					matchFailed = false;
				}
				else {
					cout << "target '" << t << "' does not exist" << endl;
				}

				if (matchFailed) {
					cout << "run matmake --help for help" << endl;
					cout << "targets: ";
					listAlternatives();
					throw runtime_error("error when parsing targets");
				}
			}
		}

		return selectedTargets;
	}

	vector<unique_ptr<IDependency>> calculateDependencies(
			vector<IBuildTarget*> selectedTargets) const {
		vector < unique_ptr < IDependency >> files;
		for (auto target : selectedTargets) {
			dout << "target " << target->name() << " src "
					<< target->get("src").concat() << endl;

			auto targetDependencies = target->calculateDependencies(*_fileHandler, targets);

			typedef decltype (files)::iterator iter_t;

			files.insert(files.end(),
					std::move_iterator<iter_t>(targetDependencies.begin()),
					std::move_iterator<iter_t>(targetDependencies.end()));

		}

		return files;
	}

	void createDirectories(const vector<unique_ptr<IDependency>>& files) const {
		set<string> directories;
		for (auto &file: files) {
			if (!file->dirty()) {
				continue;
			}
			auto dir = _fileHandler->getDirectory(file->output());
			if (!dir.empty()) {
				directories.emplace(dir);
			}
		}

		for (auto dir: directories) {
			dout << "output dir: " << dir << endl;
			if (dir.empty()) {
				continue;
			}
			if (_fileHandler->isDirectory(dir)) {
				continue; //Skip if already created
			}
			_fileHandler->createDirectory(dir);
		}
	}

	void compile(vector<string> targetArguments) override {
		dout << "compiling..." << endl;
		print();

		auto files = calculateDependencies(parseTargetArguments(targetArguments));

		for (auto& file: files) {
			file->build(*_fileHandler);
		}

		createDirectories(files);

		for (auto &file: files) {
			if (file->dirty()) {
				dout << "file " << file->output() << " is dirty" << endl;
				addTaskCount();
				file->prune();
				if (file->dependencies().empty()) {
					addTask(file.get());
				}
			}
			else {
				dout << "file " << file->output() << " is fresh" << endl;
			}
		}

		buildExternal(true, "");
		work(std::move(files));
		buildExternal(false, "");
	}

	// This is what a single thread will do
	void workThreadFunction(int i) {
		dout << "starting thread " << endl;

		workAssignMutex.lock();
		while (!tasks.empty()) {

			auto t = tasks.front();
			tasks.pop();
			workAssignMutex.unlock();
			try {
				t->work(*_fileHandler);
				stringstream ss;
				ss << "[" << getBuildProgress() << "%] ";
				vout << ss.str();
				++ this->taskFinished;
			}
			catch (MatmakeError &e) {
				cerr << e.what() << endl;
				globals.bailout = true;
			}
			printProgress();

			workAssignMutex.lock();
		}
		workAssignMutex.unlock();

		workMutex.try_lock();
		workMutex.unlock();

		dout << "thread " << i << " is finished quit" << endl;
		numberOfActiveThreads -= 1;
	}

	void workMultiThreaded() {
		vout << "running with " << globals.numberOfThreads << " threads" << endl;

		vector<thread> threads;
		numberOfActiveThreads = 0;

		auto f = [this] (int i) {
			workThreadFunction(i);
		};

		threads.reserve(globals.numberOfThreads);
		auto startTaskNum = tasks.size();
		for (size_t i = 0 ;i < globals.numberOfThreads && i < startTaskNum; ++i) {
			++ numberOfActiveThreads;
			threads.emplace_back(f, static_cast<int>(numberOfActiveThreads));
		}

		for (auto &t: threads) {
			t.detach();
		}

		while (numberOfActiveThreads > 0 && !globals.bailout) {
			workMutex.lock();
			dout << "remaining tasks " << tasks.size() << " tasks" << endl;
			dout << "number of active threads at this point " << numberOfActiveThreads << endl;
			if (!tasks.empty()) {
				std::lock_guard<mutex> guard(workAssignMutex);
				auto numTasks = tasks.size();
				if (numTasks > numberOfActiveThreads) {
					for (auto i = numberOfActiveThreads.load(); i < globals.numberOfThreads && i < numTasks; ++i) {
						dout << "Creating new worker thread to manage tasks" << endl;
						++ numberOfActiveThreads;
						thread t(f, static_cast<int>(numberOfActiveThreads));
						t.detach(); // Make the thread live without owning it
					}
				}
			}
		}
	}

	void work(vector<unique_ptr<IDependency>> files) {
		if (globals.numberOfThreads > 1) {
			workMultiThreaded();
		}
		else {
			globals.numberOfThreads = 1;
			vout << "running with 1 thread" << endl;
			while(!tasks.empty()) {
				auto t = tasks.front();
				tasks.pop();
				try {
					t->work(*_fileHandler);
				}
				catch (MatmakeError &e) {
					dout << e.what() << endl;
					globals.bailout = true;
				}
			}
		}

		for (auto& file: files) {
			if (file->dirty()) {
				cout << "file " << file->output() << " was never built" << endl;
				cout << "depending on: " << endl;
				for (auto dependency: file->dependencies()) {
					dependency->output();
				}
			}
		}

		vout << "finished" << endl;
	}

	void clean(vector<string> targetArguments) override {
		dout << "cleaning " << endl;
		auto files = calculateDependencies(parseTargetArguments(targetArguments));

		if (targetArguments.empty()) {
			buildExternal(true, "clean");

			for (auto& file: files) {
				file->clean(*_fileHandler);
			}

			buildExternal(false, "clean");
		}
		else {
			for (auto& file: files) {
				file->clean(*_fileHandler);
			}
		}

	}

	//! Show info of alternative build targets
	void listAlternatives() const override {
		for (auto &t: targets) {
			if (t->name() != "root") {
				cout << t->name() << " ";
			}
		}
		cout << "clean";
		cout << endl;
	}
};






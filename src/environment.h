// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "ienvironment.h"

#include "globals.h"
#include "token.h"
#include "ibuildtarget.h"
#include "dependency.h"
#include <queue>
#include <atomic>

#include <fstream>
#include "buildtarget.h"
#include "buildfile.h"
#include "copyfile.h"


class Environment;


class Environment: public IEnvironment {
public:
	struct ExternalMatmakeType {
		bool compileBefore;
		Token name;
		Tokens arguments;
	};

	vector<unique_ptr<BuildTarget>> targets;
	vector<unique_ptr<IDependency>> files;
	set<string> directories;
	queue<IDependency *> tasks;
	mutex workMutex;
	mutex workAssignMutex;
	std::atomic<size_t> numberOfActiveThreads;
	std::shared_ptr<IFiles> _fileHandler;
	int maxTasks = 0;
	int taskFinished = 0;
	int lastProgress = 0;
	bool finished = false;
	Globals _globals;

	std::vector<ExternalMatmakeType> externalDependencies;
	bool isDependenicesCalculated = false;

	const IFiles &fileHandler() const override {
		return *_fileHandler;
	}

	IEnvironment &env() {
		return *this;
	}

	const Globals &globals() const override {
		return _globals;
	}

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
					start({"clean"}, _globals);
				}
				else {
					cout << endl << "Building external " << dependency.name << endl;
					start(newArgs, _globals);
					if (_globals.bailout) {
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

	void addTask(IDependency *t, bool count) override {
		workAssignMutex.lock();
		tasks.push(t);
		if (count) {
			++maxTasks;
		}
		//		maxTasks = std::max((int)tasks.size(), maxTasks);
		workAssignMutex.unlock();
		workMutex.try_lock();
		workMutex.unlock();
	}

	void addTaskCount() override {
		++maxTasks;
	}

	Environment (Globals &globals, shared_ptr<IFiles> fileHandler):
		  _fileHandler(fileHandler),
		  _globals(globals)
	{
		targets.emplace_back(new BuildTarget("root", this));
	}

	BuildTarget *findTarget(Token name) override {
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

	IBuildTarget *findVariable(Token name) {
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
	Tokens getValue(NameDescriptor name) {
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
			targets.emplace_back(new BuildTarget(name, this));
			return *targets.back();
		}
	}

	void appendVariable(const NameDescriptor &name, Tokens value) override {
		(*this) [name.rootName].append(name.propertyName, value);
	}

	void setVariable(const NameDescriptor &name, Tokens value) override {
		(*this) [name.rootName].assign(name.propertyName, value) ;
	}

	void print() {
		for (auto &v: targets) {
			v->print();
		}
	}

	int getBuildProgress() override {
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

		if (!_globals.debugOutput && !_globals.verbose) {
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


	void calculateDependencies() {
		if (isDependenicesCalculated) {
			return;
		}
		for (auto &target: targets) {
			dout << "target " << target->name() << " src "
					<< target->get("src").concat() << endl;

			auto targetDependencies = target->calculateDependencies();

			typedef decltype (files)::iterator iter_t;

			files.insert(files.end(),
					std::move_iterator<iter_t>(targetDependencies.begin()),
					std::move_iterator<iter_t>(targetDependencies.end()));

		}
		isDependenicesCalculated = true;
	}



	void compile(vector<string> targetArguments) override {
		dout << "compiling..." << endl;
		print();

		calculateDependencies();
		for (auto &file: files) {
			auto dir = _fileHandler->getDirectory(file->targetPath());
			if (!dir.empty()) {
				directories.emplace(dir);
			}
		}

		for (auto &target: targets) {
			auto dir = _fileHandler->getDirectory(target->targetPath());
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


		if (targetArguments.empty()) {
			for (auto &target: targets) {
				target->build();
			}
		}
		else {
			bool matchFailed = true;
			for (auto t: targetArguments) {
				auto target = findTarget(t);
				if (target) {
					target->build();
					matchFailed = false;
				}
				else {
					cout << "target '" << t << "' does not exist" << endl;
				}

				if (matchFailed) {
					cout << "run matmake --help for help" << endl;
					cout << "targets: ";
					listAlternatives();
					_globals.bailout = true;
				}
			}
		}

		buildExternal(true, "");
		work();
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
				t->work();
				stringstream ss;
				ss << "[" << getBuildProgress() << "%] ";
				vout << ss.str();
				++ this->taskFinished;
			}
			catch (MatmakeError &e) {
				cerr << e.what() << endl;
				_globals.bailout = true;
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
		vout << "running with " << _globals.numberOfThreads << " threads" << endl;

		vector<thread> threads;
		numberOfActiveThreads = 0;

		auto f = [this] (int i) {
			workThreadFunction(i);
		};

		threads.reserve(_globals.numberOfThreads);
		auto startTaskNum = tasks.size();
		for (size_t i = 0 ;i < _globals.numberOfThreads && i < startTaskNum; ++i) {
			++ numberOfActiveThreads;
			threads.emplace_back(f, static_cast<int>(numberOfActiveThreads));
		}

		for (auto &t: threads) {
			t.detach();
		}

		while (numberOfActiveThreads > 0 && !_globals.bailout) {
			workMutex.lock();
			dout << "remaining tasks " << tasks.size() << " tasks" << endl;
			dout << "number of active threads at this point " << numberOfActiveThreads << endl;
			if (!tasks.empty()) {
				std::lock_guard<mutex> guard(workAssignMutex);
				auto numTasks = tasks.size();
				if (numTasks > numberOfActiveThreads) {
					for (auto i = numberOfActiveThreads.load(); i < _globals.numberOfThreads && i < numTasks; ++i) {
						dout << "Creating new worker thread to manage tasks" << endl;
						++ numberOfActiveThreads;
						thread t(f, static_cast<int>(numberOfActiveThreads));
						t.detach(); // Make the thread live without owning it
					}
				}
			}
		}
	}

	void work() {
		if (_globals.numberOfThreads > 1) {
			workMultiThreaded();
		}
		else {
			_globals.numberOfThreads = 1;
			vout << "running with 1 thread" << endl;
			while(!tasks.empty()) {
				auto t = tasks.front();
				tasks.pop();
				try {
					t->work();
				}
				catch (MatmakeError &e) {
					dout << e.what() << endl;
					_globals.bailout = true;
				}
			}
		}

//		for (auto& file: files) {
//			if (file->dirty()) {
//				cout << "file " << file->targetPath() << " was never built" << endl;
//				cout << "depending on: " << endl;
//				for (auto dependency: file->dependencies()) {
//					dependency->targetPath();
//				}
//			}
//		}

		vout << "finished" << endl;
	}

	void clean(vector<string> targetArguments) override {
		dout << "cleaning " << endl;
		calculateDependencies();

		if (targetArguments.empty()) {
			buildExternal(true, "clean");

			for (auto &target: targets) {
				if (target->name() != "root") {
					target->clean();
				}
			}

			buildExternal(false, "clean");
		}
		else {
			for (auto t: targetArguments) {
				auto target = findTarget(t);
				if (target) {
					target->clean();
				}
				else {
					cout << "target '" << t << "' does not exist" << endl;
				}
			}
		}

	}

	//! Show info of alternative build targets
	void listAlternatives() override {
		for (auto &t: targets) {
			if (t->name() != "root") {
				cout << t->name() << " ";
			}
		}
		cout << "clean";
		cout << endl;
	}
};






// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "ienvironment.h"

#include "globals.h"
#include "token.h"
#include "ibuildtarget.h"
#include "dependency.h"
#include <queue>

#include <fstream>
#include "buildtarget.h"



class Environment;


class CopyFile: public Dependency {
public:
	CopyFile(const CopyFile &) = delete;
	CopyFile(CopyFile &&) = delete;
	CopyFile(Token source, Token output, BuildTarget *parent, IEnvironment *env):
		  Dependency(env),
		  source(source),
		  output(output),
		  parent(parent) {}

	Token source;
	Token output;
	BuildTarget *parent;

	time_t getSourceChangedTime() {
		return env().fileHandler().getTimeChanged(source);
	}

	time_t build() override {
		if (output == source) {
			vout << "file " << output << " source and target is on same place. skipping" << endl;
		}
		auto timeChanged = getTimeChanged();

		if (getSourceChangedTime() > timeChanged) {
			queue(true);
			dirty(true);

			return time(nullptr);
		}
		return timeChanged;
	}

	void work() override {
		ifstream src(source);
		if (!src.is_open()) {
			cout << "could not open file " << source << " for copy for target " << parent->name << endl;
		}

		ofstream dst(output);
		if (!dst) {
			cout << "could not open file " << output << " for copy for target " << parent->name << endl;
		}

		vout << "copy " << source << " --> " << output << endl;
		dst << src.rdbuf();

		sendSubscribersNotice();
	}

	void clean() override {
		vout << "removing file " << output << endl;
		remove(output.c_str());
	}

	Token targetPath() override {
		return output;
	}

	bool includeInBinary() override {
		return false;
	}
};


class BuildFile: public Dependency {
	Token filename; //The source of the file
	Token output; //The target of the file
	Token depFile; //File that summarizes dependencies for file
	Token filetype; //The ending of the filename

	Token command;
	Token depCommand;

	IBuildTarget *_parent;
	vector<string> dependencies;

public:
	BuildFile(const BuildFile &) = delete;
	BuildFile(BuildFile &&) = delete;
	BuildFile(Token filename, IBuildTarget *parent, class IEnvironment *env):
		  Dependency(env),
		  filename(filename),
		  output(fixObjectEnding(parent->getBuildDirectory() + filename)),
		  depFile(fixDepEnding(parent->getBuildDirectory() + filename)),
		  filetype(stripFileEnding(filename).second),
		  _parent(parent) {
		auto withoutEnding = stripFileEnding(parent->getBuildDirectory() + filename);
		if (withoutEnding.first.empty()) {
			throw MatmakeError(filename, "could not figure out source file type '" + parent->getBuildDirectory() + filename +
											 "' . Is the file ending right?");
		}
		if (filename.empty()) {
			throw MatmakeError(filename, "empty buildfile added");
		}
		if (output.empty()) {
			throw MatmakeError(filename, "could not find target name");
		}
		if (depFile.empty()) {
			throw MatmakeError(filename, "could not find dep filename");
		}
	}


	static Token fixObjectEnding(Token filename) {
		return stripFileEnding(filename).first + ".o";
	}
	static Token fixDepEnding(Token filename) {
		return stripFileEnding(filename).first + ".d";
	}

	time_t getInputChangedTime() {
		return env().fileHandler().getTimeChanged(filename);
	}

	time_t getDepFileChangedTime() {
		return env().fileHandler().getTimeChanged(depFile);
	}

	vector<string> parseDepFile() {
		if (depFile.empty()) {
			return {};
		}
		ifstream file(depFile);
		if (file.is_open()) {
			vector <string> ret;
			string d;
			file >> d; //The first is the target path
			while (file >> d) {
				if (d != "\\") {
					ret.push_back(d);
				}
			}
			return ret;
		}
		else {
			dout << "could not find .d file for " << output << " --> " << depFile << endl;
			return {};
		}
	}

	time_t build() override {
		auto flags = _parent->get("flags").concat();
		if (filetype == "cpp") {
			auto cppflags = _parent->get("cppflags");
			if (!cppflags.empty()) {
				flags += (" " + cppflags.concat());
			}
		}
		if (filetype == "c") {
			auto cflags = _parent->get("cflags");
			if (!cflags.empty()) {
				flags += (" " + cflags.concat());
			}
		}
		bool shouldQueue = false;

		auto depChangedTime = getDepFileChangedTime();
		auto inputChangedTime = getInputChangedTime();

		auto dependencyFiles = parseDepFile();

		if (depChangedTime == 0 || inputChangedTime > depChangedTime || dependencyFiles.empty()) {
			depCommand = _parent->getCompiler(filetype) + " -MT " + output + " -MM " + filename + " " + flags + " -w";
			depCommand.location = filename.location;
			shouldQueue = true;
		}

		auto timeChanged = getTimeChanged();


		for (auto &d: dependencyFiles) {
			auto dependencyTimeChanged = env().fileHandler().getTimeChanged(d);
			if (dependencyTimeChanged == 0 || dependencyTimeChanged > timeChanged) {
				dirty(true);
				break;
			}
		}

		if (shouldQueue || dirty()) {
			dirty(true);
			queue(true);
		}

		if (dirty()) {
			command = _parent->getCompiler(filetype) + " -c -o " + output + " " + filename + " " + flags;
			command.location = filename.location;

			return time(nullptr);
		}
		else {
			return getTimeChanged();
		}
	}

	void work() override {
		if (!depCommand.empty()) {
			vout << depCommand << endl;
			pair <int, string> res = env().fileHandler().popenWithResult(depCommand);
			if (res.first) {
				throw MatmakeError(depCommand, "could not build dependencies:\n" + depCommand + "\n" + res.second);
			}
			else {
				try {
					ofstream(depFile) << res.second;
				}
				catch (runtime_error &e) {
					throw MatmakeError(depCommand, "could not write to file " + depFile + "\n" + e.what());
				}
			}
		}

		if (!command.empty()) {
			vout << command << endl;
			pair <int, string> res = env().fileHandler().popenWithResult(command);
			if (res.first) {
				throw MatmakeError(command, "could not build object:\n" + command + "\n" + res.second);
			}
			else if (!res.second.empty()) {
				cout << (command + "\n" + res.second + "\n") << std::flush;
			}
			dirty(false);
			sendSubscribersNotice();
		}

	}

	Token targetPath() override {
		return output;
	}

	void clean() override {
		vout << "removing file " << targetPath() << endl;
		remove(targetPath().c_str());
		if (!depFile.empty()) {
			vout << "removing file " << depFile << endl;
			remove(depFile.c_str());
		}
	}
};


class Environment: public IEnvironment {
public:
	vector<unique_ptr<BuildTarget>> targets;
	vector<unique_ptr<Dependency>> files;
	set<string> directories;
	queue<IDependency *> tasks;
	mutex workMutex;
	mutex workAssignMutex;
	std::atomic<size_t> numberOfActiveThreads;
	std::unique_ptr<IFiles> _fileHandler;
	int maxTasks = 0;
	int taskFinished = 0;
	int lastProgress = 0;
	bool finished = false;
	Globals _globals;

	IFiles &fileHandler() override {
		return *_fileHandler;
	}

	IEnvironment &env() {
		return *this;
	}

	Globals &globals() override {
		return _globals;
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

	Environment (Globals &globals, IFiles *fileHandler):
		  _fileHandler(fileHandler),
		  _globals(globals)
	{
		targets.emplace_back(new BuildTarget("root", this));
	}

	IBuildTarget *findTarget(Token name) override {
		if (name.empty()) {
			return nullptr;
		}
		for (auto &v: targets) {
			if (v->name == name) {
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
			if (v->name == name) {
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

	void appendVariable(NameDescriptor name, Tokens value) {
		(*this) [name.rootName].append(name.propertyName, value);
	}

	void setVariable(NameDescriptor name, Tokens value) {
		(*this) [name.rootName].assign(name.propertyName, value) ;
	}

	void print() {
		for (auto &v: targets) {
			v->print();
		}
	}

	void printProgress() {
		if (!maxTasks) {
			return;
		}
		int amount = (taskFinished * 100 / maxTasks);

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
			ss << "[" << amount << "%] ";
		}

		cout << ss.str();
		cout.flush();
	}


	void calculateDependencies() {
		files.clear();
		for (auto &target: targets) {
			auto outputPath = target->getOutputDir();

			target->print();
			dout << "target " << target->name << " src " << target->get("src").concat() << endl;
			for (auto &filename: target->getGroups("src")) {
				if (filename.empty()) {
					continue;
				}
				files.emplace_back(new BuildFile(filename, target.get(), this));
				target->addDependency(files.back().get());
			}
			for (auto &file: target->getGroups("copy")) {
				if (file.empty()) {
					continue;
				}
				files.emplace_back(new CopyFile(file, outputPath + "/" + file, target.get(), this));
				target->addDependency(files.back().get());
			}
		}
	}

	void compile(vector<string> targetArguments) {
		print();

		calculateDependencies();
		for (auto &file: files) {
			auto dir = _fileHandler->getDirectory(file->targetPath());
			if (dir.empty()) {
				continue;
			}
			directories.emplace(dir);
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

		work();
	}

	void work() {
		vector<thread> threads;

		if (_globals.numberOfThreads < 2) {
			_globals.numberOfThreads = 1;
			vout << "running with 1 thread" << endl;
		}
		else {
			vout << "running with " << _globals.numberOfThreads << " threads" << endl;
		}

		if (_globals.numberOfThreads > 1) {
			numberOfActiveThreads = 0;
			auto f = [&] (int i) {
				dout << "starting thread " << endl;

				workAssignMutex.lock();
				while (!tasks.empty()) {

					auto t = tasks.front();
					tasks.pop();
					workAssignMutex.unlock();
					try {
						t->work();
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
							t.detach();
						}
					}
				}
			}
		}
		else {
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

		vout << "finished" << endl;
	}

	void clean(vector<string> targetArguments) {
		calculateDependencies();

		if (targetArguments.empty()) {
			for (auto &target: targets) {
				target->clean();
			}
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

	// Show info of alternative build targets
	void listAlternatives() {
		for (auto &t: targets) {
			if (t->name != "root") {
				cout << t->name << " ";
			}
		}
		cout << "clean";
		cout << endl;
	}
};






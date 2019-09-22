/*
 * matmake.cpp
 *
 *  Created on: 9 apr. 2019
 *      Author: Mattias Lasersköld
 */


#include <cstdlib>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <map>
#include <dirent.h>
#include <set>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>

#include "files.h"
#include "token.h"

using namespace std;

bool verbose = false;
bool debugOutput = false;

namespace {

auto numberOfThreads = thread::hardware_concurrency(); //Get the maximal number of threads
bool bailout = false; // when true: exit the program in a controlled way

}

struct NameDescriptor {
	NameDescriptor(std::vector<Token> name) {
		if (name.size() == 1) {
			memberName = name.front();
		}
		else if (name.size() == 3 && name[1] == ".") {
			rootName = name.front();
			memberName = name.back();
		}
	}

	NameDescriptor(const Token &name, const Token &memberName): rootName(name), memberName(memberName) {}

	bool empty() {
		return memberName.empty();
	}

	Token rootName = "root";
	Token memberName = "";
};


class Environment;

class Dependency {
public:
    Dependency(Environment *env): _env(env) {}

	virtual ~Dependency() {}

	time_t getTimeChanged() {
		return ::getTimeChanged(targetPath());
	}

	virtual time_t build() = 0;
	virtual void work() = 0;

	void addDependency(Dependency *file) {
        _dependencies.insert(file);
    }

	// Add task to list of tasks to be executed
	// the count variable sepcify if the dependency should be counted (true)
	// if hintStatistic has already been called
	void queue(bool count);

	// Tell the environment that there will be a dependency added in the future
	// This is when the dependency is waiting for other files to be made
	void hintStatistic();

	virtual Token targetPath() = 0;

	virtual void clean() = 0;

    virtual bool includeInBinary() { return true; }

	virtual void addSubscriber(Dependency *s) {
		lock_guard<mutex> guard(accessMutex);
        if (find(_subscribers.begin(), _subscribers.end(), s) == _subscribers.end()) {
            _subscribers.push_back(s);
		}
	}

	//Send a notice to all subscribers
	virtual void sendSubscribersNotice() {
		lock_guard<mutex> guard(accessMutex);
        for (auto s: _subscribers) {
			s->notice(this);
		}
        _subscribers.clear();
	}

	// A message from a object being subscribed to
	// This is used by targets to know when all dependencies
	// is built
    virtual void notice(Dependency *) {}

    void lock() {
        accessMutex.lock();
    }

    void unlock() {
        accessMutex.unlock();
    }

    bool dirty() { return _dirty; }
    void dirty(bool value) { _dirty = value; }

    const set<class Dependency*> dependencies() const { return _dependencies; }
    const vector <Dependency*> & subscribers() const { return _subscribers; }

    Environment *environment() { return _env; }

private:
    Environment *_env;
    set<class Dependency*> _dependencies;
    vector <Dependency*> _subscribers;
    mutex accessMutex;
    bool _dirty = false;
};


struct BuildTarget: public Dependency {
    map<Token, Tokens> members;
    Token name;
    set<Dependency *> waitList;
	Token command;

	BuildTarget(Token name, class Environment *env): Dependency(env), name(name) {
		if (name != "root") {
			assign("inherit", Token("root"));
		}
		else {
//			assign("exe", Token("program"));
			assign("cpp", Token("c++"));
			assign("cc", Token("cc"));
		}
    }

	BuildTarget(class Environment *env): Dependency(env){
		assign("inherit", Token("root"));
    }

	BuildTarget(NameDescriptor n, Tokens v, Environment *env): BuildTarget(n.rootName, env) {
		assign(n.memberName, v);
	}

	void inherit(const BuildTarget &parent) {
		for (auto v: parent.members) {
			if (v.first == "inherit") {
				continue;
			}
			assign(v.first, v.second);
		}
	}

	void assign(Token memberName, Tokens value) {
		members[memberName] = value;

		if (memberName == "inherit") {
			auto parent = getParent();
			if (parent) {
				inherit(*parent);
			}
		}
		else if (memberName == "exe" || memberName == "dll") {
			if (trim(value.concat()) == "%") {
				members[memberName] = name;
			}
		}

		if (memberName == "dll") {
			auto n = members[memberName];
			if (!n.empty()) {
				auto ending = stripFileEnding(n.concat(), true);
				if (!ending.second.empty()) {
					members[memberName] = Token(ending.first + ".so");
				}
				else {
					members[memberName] = Token(n.concat() + ".so");
				}
			}
		}
	}

	void append(Token memberName, Tokens value) {
		members[memberName].append(value);
	}

	Tokens get(const NameDescriptor &name) {
		return get(name.memberName);
	}

	Tokens get(const Token &memberName) {
		try {
			return members.at(memberName);
		}
        catch (out_of_range &) {
			return {};
		}
	}

	Tokens getGroups(const Token &memberName) {
		auto sourceString = get(memberName);

		auto groups = sourceString.groups();

		Tokens ret;
		for (auto g: groups) {
			auto sourceFiles = findFiles(g.concat());
			ret.insert(ret.end(), sourceFiles.begin(), sourceFiles.end());
		}

		if (ret.size() == 1) {
			if (ret.front().empty()) {
				vout << " no pattern matching for " << memberName << endl;
			}
		}
		return ret;
	}

    BuildTarget *getParent() ;

	void print() {
		vout << "target " << name << ": " << endl;
		for (auto &m: members) {
			vout << "\t" << m.first << " = " << m.second << " " << endl;
		}
		vout << endl;
	}

	Token getCompiler(Token filetype) {
		if (filetype == "cpp") {
			return get("cpp").concat();
		}
		else if (filetype == "c") {
			return get("cc").concat();
		}
		else {
			return "echo";
		}
	}


	time_t build() override {
        dirty(false);
		auto exe = targetPath();
		if (exe.empty() || name == "root") {
			return 0;
		}

		vout << endl;
		vout << "  target " << name << "..." << endl;

        time_t lastDependency = 0;
        for (auto &d:dependencies()) {
            auto t = d->build();
            if (d->dirty()) {
				d->addSubscriber(this);
                lock_guard<Dependency> g(*this);
                waitList.insert(d);
			}
			if (t > lastDependency) {
				lastDependency = t;
			}
			if (t == 0) {
                dirty(true);
				break;
			}
		}

		if (lastDependency > getTimeChanged()) {
            dirty(true);
        }
        else if (!dirty()) {
			cout << name << " is fresh " << endl;
		}

        if (dirty()) {
			Token fileList;

            for (auto f: dependencies()) {
				if (f->includeInBinary()) {
					fileList += (f->targetPath() + " ");
				}
			}

			auto cpp = getCompiler("cpp");
			if (!get("dll").empty()) {
				command = cpp + " -shared -o " + exe + " -Wl,--start-group " + fileList + " " + get("libs").concat() + "  -Wl,--end-group  " + get("flags").concat();
			}
			else {
				command = cpp + " -o " + exe + " -Wl,--start-group " + fileList + " " + get("libs").concat() + "  -Wl,--end-group  " + get("flags").concat();
			}
			command.location = cpp.location;

			if (waitList.empty()) {
				queue(true);
			}
			else {
				hintStatistic();
			}

            return time(nullptr);
		}

		return getTimeChanged();
	}

	void notice(Dependency * d) override {
        {
            lock_guard<Dependency> g(*this);
            waitList.erase(waitList.find(d));
        }
		vout << d->targetPath() << " removed from wating list from " << name << " " << waitList.size() << " to go" << endl;
		if (waitList.empty()) {
			queue(false);
		}
	}

	// This is called when all dependencies are built
	void work() override {
		vout << "linking " << name << endl;
		vout << command << endl;
		pair <int, string> res = popenWithResult(command);
		if (res.first) {
			throw MatmakeError(command, "linking failed with\n" + command + "\n" + res.second);
		}
		else if (!res.second.empty()) {
			cout << (command + "\n" + res.second + "\n") << flush;
		}
        dirty(false);
		sendSubscribersNotice();
		vout << endl;
	}

	void clean() override {
        for (auto &d: dependencies()) {
			d->clean();
		}
		vout << "removing file " << targetPath() << endl;
		remove(targetPath().c_str());
	}


	Token targetPath() override {
		auto dir = get("dir").concat();
		if (!dir.empty()) {
			dir += "/";
		}
		auto exe = get("exe").concat();
		if (exe.empty()) {
			auto dll = get("dll").concat();
			if (dll.empty()) {
				// Automatically create target name
				dout << "target missing: automatically sets " << name << " as exe" << endl;
				return dir + name;
			}
			else {
				return dir + dll;
			}
		}
		else {
			return dir + exe;
		}
	}

	Token getOutputDir() {
		auto outputPath = get("dir").concat();
		if (!outputPath.empty()) {
			outputPath += "/";
		}
		return outputPath;
	}

	//If set, where the obj-files is placed
	Token getObjDir() {
		auto outputPath = get("objdir").concat();
		if (!outputPath.empty()) {
			outputPath += "/";
		}
		else {
			return getOutputDir();
		}
		return outputPath;
	}
};


class CopyFile: public Dependency {
public:
    CopyFile(const CopyFile &) = delete;
    CopyFile(CopyFile &&) = delete;
	CopyFile(Token source, Token output, BuildTarget *parent, Environment *env):
		Dependency(env),
		source(source),
		output(output),
		parent(parent) {}

    Token source;
    Token output;
    BuildTarget *parent;

	time_t getSourceChangedTime() {
		return ::getTimeChanged(source);
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
public:
    BuildFile(const BuildFile &) = delete;
    BuildFile(BuildFile &&) = delete;
	BuildFile(Token filename, BuildTarget *parent, class Environment *env):
		Dependency(env),
		filename(filename),
		output(fixObjectEnding(parent->getObjDir() + filename)),
		depFile(fixDepEnding(parent->getObjDir() + filename)),
		filetype(stripFileEnding(filename).second),
		parent(parent) {
		auto withoutEnding = stripFileEnding(parent->getObjDir() + filename);
		if (withoutEnding.first.empty()) {
			throw MatmakeError(filename, "could not figure out source file type '" + parent->getObjDir() + filename +
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
	Token filename; //The source of the file
	Token output; //The target of the file
	Token depFile; //File that summarizes dependencies for file
	Token filetype; //The ending of the filename

	Token command;
	Token depCommand;

	BuildTarget *parent;
	vector<string> dependencies;


	static Token fixObjectEnding(Token filename) {
		return stripFileEnding(filename).first + ".o";
	}
	static Token fixDepEnding(Token filename) {
		return stripFileEnding(filename).first + ".d";
	}

	time_t getInputChangedTime() {
		return ::getTimeChanged(filename);
	}

	time_t getDepFileChangedTime() {
		return ::getTimeChanged(depFile);
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
		auto flags = parent->get("flags").concat();
		if (filetype == "cpp") {
			auto cppflags = parent->get("cppflags");
			if (!cppflags.empty()) {
				flags += (" " + cppflags.concat());
			}
		}
		if (filetype == "c") {
			auto cflags = parent->get("cflags");
			if (!cflags.empty()) {
				flags += (" " + cflags.concat());
			}
		}
		bool shouldQueue = false;

		auto depChangedTime = getDepFileChangedTime();
		auto inputChangedTime = getInputChangedTime();

		auto dependencyFiles = parseDepFile();

		if (depChangedTime == 0 || inputChangedTime > depChangedTime || dependencyFiles.empty()) {
			depCommand = parent->getCompiler(filetype) + " -MT " + output + " -MM " + filename + " " + flags + " -w";
			depCommand.location = filename.location;
			shouldQueue = true;
		}

		auto timeChanged = getTimeChanged();


		for (auto &d: dependencyFiles) {
			auto dependencyTimeChanged = ::getTimeChanged(d);
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
			command = parent->getCompiler(filetype) + " -c -o " + output + " " + filename + " " + flags;
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
			pair <int, string> res = popenWithResult(depCommand);
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
			pair <int, string> res = popenWithResult(command);
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


class Environment {
public:
	vector<unique_ptr<BuildTarget>> targets;
	vector<unique_ptr<Dependency>> files;
	set<string> directories;
	queue<Dependency *> tasks;
	mutex workMutex;
	mutex workAssignMutex;
	std::atomic<size_t> numberOfActiveThreads;
	int maxTasks = 0;
	int taskFinished = 0;
	int lastProgress = 0;
	bool finished = false;

	void addTask(Dependency *t, bool count) {
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

	void addTaskCount() {
		++maxTasks;
	}

	Environment () {
		targets.emplace_back(new BuildTarget("root", this));
	}

	BuildTarget *findTarget(Token name) {
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

	BuildTarget *findVariable(Token name) {
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

	Tokens getValue(NameDescriptor name) {
		if (auto variable = findVariable(name.rootName)) {
			return variable->get(name.memberName);
		}
		else {
			return {};
		}
	}

	BuildTarget &operator[] (Token name) {
		if (auto v = findVariable(name)) {
			return *v;
		}
		else {
			targets.emplace_back(new BuildTarget(name, this));
			return *targets.back();
		}
	}

	void appendVariable(NameDescriptor name, Tokens value) {
		(*this) [name.rootName].append(name.memberName, value);
	}

	void setVariable(NameDescriptor name, Tokens value) {
		(*this) [name.rootName].assign(name.memberName, value) ;
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

		if (!debugOutput && !verbose) {
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
			auto dir = getDirectory(file->targetPath());
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
			if (isDirectory(dir)) {
				continue; //Skip if already created
			}
			createDirectory(dir);
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
					bailout = true;
				}
			}
		}

		work();
	}

	void work() {
		vector<thread> threads;

		if (numberOfThreads < 2) {
			numberOfThreads = 1;
			vout << "running with 1 thread" << endl;
		}
		else {
			vout << "running with " << numberOfThreads << " threads" << endl;
		}

		if (numberOfThreads > 1) {
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
						bailout = true;
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

			threads.reserve(numberOfThreads);
			auto startTaskNum = tasks.size();
			for (size_t i = 0 ;i < numberOfThreads && i < startTaskNum; ++i) {
				++ numberOfActiveThreads;
				threads.emplace_back(f, static_cast<int>(numberOfActiveThreads));
			}

			for (auto &t: threads) {
				t.detach();
			}

			while (numberOfActiveThreads > 0 && !bailout) {
				workMutex.lock();
				dout << "remaining tasks " << tasks.size() << " tasks" << endl;
				dout << "number of active threads at this point " << numberOfActiveThreads << endl;
				if (!tasks.empty()) {
					std::lock_guard<mutex> guard(workAssignMutex);
					auto numTasks = tasks.size();
					if (numTasks > numberOfActiveThreads) {
						for (auto i = numberOfActiveThreads.load(); i < numberOfThreads && i < numTasks; ++i) {
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
					bailout = true;
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



BuildTarget *BuildTarget::getParent() {
    auto inheritFrom = get("inherit").concat();
    return environment()->findTarget(inheritFrom);
}


void Dependency::queue(bool count) {
    _env->addTask(this, count);
}

void Dependency::hintStatistic() {
    _env->addTaskCount();
}

bool isOperator(string &op) {
	vector<string> opList = {
		"=",
		"+=",
		"-=",
	};
	return find(opList.begin(), opList.end(), op) != opList.end();
}



namespace {

const char * helpText = R"_(
Matmake

A fast simple build system. It can be installed on the system or included with the source code

arguments:
[target]          build only specified target eg. debug or release
clean             remove all target files
clean [target]    remove specified target files
--local           do not build external dependencies (other folders)
-v or --verbose   print more information on what is happening
-d or --debug     print debug messages
--list -l         print a list of available targets
-j [n]            use [n] number of threads
-j 1              run in single thread mode
--help or -h      print this text
--init            create a cpp project in current directory
--init [dir]      create a cpp project in the specified directory
--example         show a example of a Matmakefile


Matmake defaults to use optimal number of threads for the computer to match the number of cores.
)_";


const char * exampleMain = R"_(
#include <iostream>

using namespace std;

int main(int argc, char **argv) {
	cout << "Hello" << endl;
	
	return 0;
}

)_";

const char *exampleMatmakefile = R"_(
# Matmake file
# https://github.com/mls-m5/matmake

cppflags += -std=c++14      # c++ only flags
cflags +=                   # c only flags 

# global flags:
flags += -W -Wall -Wno-unused-parameter -Wno-sign-compare #-Werror

## Main target
main.flags += -Iinclude
main.src = 
	src/*.cpp
	# multi line values starts with whitespace
# main.libs += # -lGL -lSDL2 # libraries to add at link time

# main.exe = main          # name of executable (not required)
# main.dir = bin/release   # set build path
# main.objdir = bin/obj    # separates obj-files from build files
# main.dll = lib           # use this instead of exe to create so/dll file
# external tests           # call matmake or make in another folder

)_";

const char *exampleMakefile = R"_(
# build settings is in file 'Matmakefile'


all:
	@echo using Matmake buildsystem
	@echo for more options use 'matmake -h'
	matmake

clean:
	matmake clean
	
)_";

}

int createProject(string dir) {
	if (!dir.empty()) {
		createDirectory(dir);

		if (chdir(dir.c_str())) {
			cerr << "could not create directory " << dir << endl;
			return -1;
		}
	}

	if (getTimeChanged("Matmakefile") > 0) {
		cerr << "Matmakefile already exists. Exiting..." << endl;
		return -1;
	}

	{
		ofstream file("Matmakefile");
		file << exampleMatmakefile;
	}


	if (getTimeChanged("Makefile") > 0) {
		cerr << "Makefile already exists. Exiting..." << endl;
		return -1;
	}

	{
		ofstream file("Makefile");
		file << exampleMakefile;
	}

	createDirectory("src");

	if (getTimeChanged("src/main.cpp") > 0) {
		cerr << "src/main.cpp file already exists. Exiting..." << endl;
		return -1;
	}

	createDirectory("include");

	{
		ofstream file("src/main.cpp");
		file << exampleMain;
	}

	cout << "project created";
	if (!dir.empty()) {
		cout << " in " << dir;
	}
	cout << "..." << endl;
	return -1;
}


int start(vector<string> args) {
	auto startTime = time(nullptr);
	ifstream matmakefile("Matmakefile");

	string line;
	Environment environment;
	vector<string> targets;

	string operation = "build";
	bool localOnly = false;

	auto argc = args.size(); //To support old code
	for (size_t i = 0; i < args.size(); ++i) {
		string arg = args[i];
		if (arg == "clean") {
			operation = "clean";
		}
		else if(arg == "--local") {
			localOnly = true;
		}
		else if (arg == "-v" || arg == "--verbose") {
			verbose = true;
		}
		else if (arg == "--list" || arg == "-l") {
			operation = "list";
		}
		else if (arg == "--help" || arg == "-h") {
			cout << helpText << endl;
			return 0;
		}
		else if (arg == "--example") {
			cout << "### Example matmake file (Matmakefile): " << endl;
			cout << "# means comment" << endl;
			cout << exampleMatmakefile << endl;
			cout << "### Example main file (src/main.cpp):" << endl;
			cout << exampleMain << endl;
			return 0;
		}
		else if (arg == "--debug" || arg == "-d") {
			debugOutput = true;
			verbose = true;
		}
		else if (arg == "-j") {
			++i;
			if (i < argc) {
				numberOfThreads = static_cast<unsigned>(atoi(args[i].c_str()));
			}
			else {
				cerr << "expected number of threads after -j argument" << endl;
				return -1;
			}
		}
		else if (arg == "--init") {
			++i;
			if (i < argc) {
				string arg = args[i];
				if (arg[0] != '-') {
					return createProject(args[i]);
				}
			}
			return createProject("");
		}
		else if (arg == "all") {
			//Do nothing. Default is te build all
		}
		else {
			targets.push_back(arg);
		}
	}

	if (!matmakefile.is_open()) {
		if (getTimeChanged("Makefile") || getTimeChanged("makefile")) {
			cout << "makefile in " << getCurrentWorkingDirectory() << endl;
			string arguments = "make";
			for (auto arg: args) {
				arguments += (" " + arg);
			}
			arguments += " -j";
			system(arguments.c_str());
			cout << endl;
		}
		else {
			cout << "matmake: could not find Matmakefile in " << getCurrentWorkingDirectory() << endl;
		}
		return -1;
	}

	int lineNumber = 1;

	auto getMultilineArgument = [&]() {
		Tokens ret;
		string line;
		// Continue as long as the line starts with a space character
		while (matmakefile && isspace(matmakefile.peek())) {
			getline(matmakefile, line);
			auto words = tokenize(line, lineNumber);
			ret.append(words);
			if (!ret.empty()) {
				ret.back().trailingSpace += " ";
			}
		}

		return ret;
	};

	while (matmakefile) {
		getline(matmakefile, line);
		auto words = tokenize(line, lineNumber);

		if (!words.empty()) {
			auto it = words.begin() + 1;
			for (; it != words.end(); ++it) {
				if (isOperator(*it)) {
					break;
				}
			}
			if (it != words.end()) {
				auto argumentStart = it + 1;
				vector<Token> variableName(words.begin(), it);
				Tokens value(argumentStart, words.end());
				auto variableNameString = concatTokens(words.begin(), it);

				if (value.empty()) {
					value = getMultilineArgument();
				}

				if (*it == "=") {
					environment.setVariable(variableName, value);
				}
				else if (*it == "+=") {
					environment.appendVariable(variableName, value);
				}
			}
			else if (!localOnly && words.size() >= 2 && words.front() == "external") {
				cout << "external dependency to " << words[1] << endl;

				auto currentDirectory = getCurrentWorkingDirectory();

				if (!chdir(words[1].c_str())) {
					vector <string> newArgs(words.begin() + 2, words.end());

					if (operation == "clean") {
						start(args);
					}
					else {
						start(newArgs);
						if (bailout) {
							break;
						}
						cout << endl;
					}
				}
				else {
					cerr << "could not open directory " << words[1] << endl;
				}

				if (chdir(currentDirectory.c_str())) {
					throw runtime_error("could not go back to original working directory " + currentDirectory);
				}
				else {
					vout << "returning to " << currentDirectory << endl;
					vout << endl;
				}
			}
		}
		++lineNumber;
	}

	if (!bailout) {
		try {
			if (operation == "build") {
				environment.compile(targets);
			}
			else if (operation == "list") {
				environment.listAlternatives();
				return 0;
			}
			else if (operation == "clean") {
				environment.clean(targets);
			}
		}
		catch (MatmakeError &e) {
			cerr << e.what() << endl;
		}
	}
	auto endTime = time(nullptr);

	auto duration = endTime - startTime;
	auto m = duration / 60;
	auto s = duration - m * 60;

	cout << endl;
	if (bailout) {
		cout << "failed... " << m << "m " << s << "s" << endl;
		return -1;
	}
	else {
		string doneMessage = "done...";
		if (operation == "clean") {
			doneMessage = "cleaned...";
		}
		cout << doneMessage << " " << m << "m " << s << "s" << endl;
		return 0;
	}
}


int main(int argc, char **argv) {
	vector<string> args(argv + 1, argv + argc);
	start(args);
}


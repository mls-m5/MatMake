/*
 * matmake.cpp
 *
 *  Created on: 9 apr. 2019
 *      Author: Mattias Lasersköld
 *
 * The main file to be compiled
 * Notice that the design pattern of this file is not to be used
 * for inspiration in any actual program. The single source file design
 * is simply chosen to be able to compile the whole build system with one
 * command line on any system.
 *
 * To build the projekt run "make" in parent directory, or run eqvivalent to
 * `c++ matmake.cpp -o ../matmake`
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
#include <set>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>

#include "files.h"
#include "token.h"
#include "ienvironment.h"
#include "buildtarget.h"
#include "dependency.h"
#include "buildtarget.h"

#include "globals-definition.h" // Global variables

using namespace std;

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


class Environment: IEnvironment {
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

	IFiles &fileHandler() override {
		return *_fileHandler;
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

	Environment (IFiles *fileHandler):
		  _fileHandler(fileHandler){
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
	Files files;
	if (!dir.empty()) {
		createDirectory(dir);

		if (chdir(dir.c_str())) {
			cerr << "could not create directory " << dir << endl;
			return -1;
		}
	}

	if (files.getTimeChanged("Matmakefile") > 0) {
		cerr << "Matmakefile already exists. Exiting..." << endl;
		return -1;
	}

	{
		ofstream file("Matmakefile");
		file << exampleMatmakefile;
	}


	if (files.getTimeChanged("Makefile") > 0) {
		cerr << "Makefile already exists. Exiting..." << endl;
		return -1;
	}

	{
		ofstream file("Makefile");
		file << exampleMakefile;
	}

	createDirectory("src");

	if (files.getTimeChanged("src/main.cpp") > 0) {
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
	Environment environment(new Files());
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
		if (environment.fileHandler().getTimeChanged("Makefile") || environment.fileHandler().getTimeChanged("makefile")) {
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


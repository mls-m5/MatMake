//Copyright Mattias Larsson Sköld 2019

#pragma once


#include "files.h"
#include "token.h"
#include "ienvironment.h"
#include "buildtarget.h"
#include "dependency.h"
#include "buildtarget.h"
#include "environment.h"

#include "globals.h" // Global variables

using namespace std;

bool isOperator(string &op) {
	vector<string> opList = {
		"=",
		"+=",
		"-=",
		};
	return find(opList.begin(), opList.end(), op) != opList.end();
}

typedef bool IsErrorT;
typedef bool ShouldQuitT;

namespace {

const char * helpText = R"_(
Matmake

A fast simple build system. It can be installed on the system or included with the source code

arguments:
[target]          build only specified target eg. debug or release
clean             remove all target files
clean [target]    remove specified target files
rebuild           first run clean and then build
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
main.includes +=
    include
# main.flags += -g         # extra compiler flags for target
main.src =
    src/*.cpp
    # multi line values starts with whitespace
# main.libs += # -lGL -lSDL2 # libraries to add at link time

# main.out = main          # name of executable (not required)
# main.out = shared main   # create a shared library (dll/so)
# main.link = [libname]    # link to shared or static library with targetname libname
# main.dir = bin/release   # set build path
# main.objdir = bin/obj    # separates obj-files from build files
# main.sysincludes +=      # include files that should not show errors
# main.define += X         # define macros in program like #define
# external tests           # call matmake or make in another folder after build
# dependency some-dir      # same as external but before build in the current dir

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


//! Creates a project in the current folder
//!
//! @returns 0 on success anything else on error
int createProject(string dir) {
	Files files;
	if (!dir.empty()) {
		files.createDirectory(dir);

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

	files.createDirectory("src");

	if (files.getTimeChanged("src/main.cpp") > 0) {
		cerr << "src/main.cpp file already exists. Exiting..." << endl;
		return -1;
	}

	files.createDirectory("include");

	{
		ofstream file("src/main.cpp");
		file << exampleMain;
	}

	cout << "project created";
	if (!dir.empty()) {
		cout << " in " << dir;
	}
	cout << "..." << endl;
	return 0;
}


IsErrorT tokenizeMatmakeFile() {
	ifstream matmakefile("Matmakefile");

	if (!matmakefile.is_open()) {
		return true;
	}

	for (int lineNumber = 1;
		   matmakefile;
		   ++lineNumber) {
		std::string line;
		getline(matmakefile, line);
		auto words = tokenize(line, lineNumber);

		for (auto word: words) {
			cout << word.location.line << ":"
				 << word.location.col << " " << word
				 << " '"  << word.trailingSpace << "'" << endl;
		}
	}

	return false;
}


//! Variables that is to be used by each start function
struct Locals {
	vector<string> targets; // What to build
	vector<string> args; // Copy of command line arguments
	vector<string> externalDependencies; // Dependencies to build before main build
	vector<string> external; // External dependencies to build after main build
	string operation = "build";
	bool localOnly = false;
};

//! Parse arguments and produce a new Locals object containing the result
//!
//! Also alters globals object if any options related to that is used
std::tuple<Locals, ShouldQuitT, IsErrorT> parseArguments(vector<string> args, Globals &globals) {
	Locals locals;
	ShouldQuitT shouldQuit = false;
	IsErrorT isError = false;

	locals.args = args;

	for (size_t i = 0; i < args.size(); ++i) {
		string arg = args[i];
		if (arg == "clean") {
			locals.operation = "clean";
		}
		else if (arg == "rebuild") {
			locals.operation = "rebuild";
		}
		else if(arg == "--local") {
			locals.localOnly = true;
		}
		else if (arg == "-v" || arg == "--verbose") {
			globals.verbose = true;
		}
		else if (arg == "--list" || arg == "-l") {
			locals.operation = "list";
		}
		else if (arg == "--help" || arg == "-h") {
			cout << helpText << endl;
			shouldQuit = true;
			break;
		}
		else if (arg == "--example") {
			cout << "### Example matmake file (Matmakefile): " << endl;
			cout << "# means comment" << endl;
			cout << exampleMatmakefile << endl;
			cout << "### Example main file (src/main.cpp):" << endl;
			cout << exampleMain << endl;
			shouldQuit = true;
			break;
		}
		else if (arg == "--debug" || arg == "-d") {
			globals.debugOutput = true;
			globals.verbose = true;
		}
		else if (arg == "-j") {
			++i;
			if (i < args.size()) {
				globals.numberOfThreads = static_cast<unsigned>(atoi(args[i].c_str()));
			}
			else {
				cerr << "expected number of threads after -j argument" << endl;
				isError = true;
				break;
			}
		}
		else if (arg == "--init") {
			++i;
			if (i < args.size()) {
				string arg = args[i];
				if (arg[0] != '-') {
					isError = createProject(args[i]);
					break;
				}
			}
			isError = createProject("");
			break;
		}
		else if (arg == "all") {
			//Do nothing. Default is te build all
		}
		else if (arg == "--tokenize") {
			// For debugging
			tokenizeMatmakeFile();
			shouldQuit = true;
			break;
		}
		else {
			locals.targets.push_back(arg);
		}
	}

	return {locals, shouldQuit, isError};
}

//! Parse the Matmakefile (obviously). Put result in environment.
std::tuple<ShouldQuitT, IsErrorT> parseMatmakeFile(const Locals& locals,
                                                   const Globals &globals,
                                                   IEnvironment &environment) {
    ShouldQuitT shouldQuit = false;
    IsErrorT isError = false;
    auto env = [&] () -> IEnvironment& { return environment; }; // For dout and vout
	auto &files = environment.fileHandler();

	ifstream matmakefile("Matmakefile");
	if (!matmakefile.is_open()) {
		if (environment.fileHandler().getTimeChanged("Makefile") || environment.fileHandler().getTimeChanged("makefile")) {
			cout << "makefile in " << files.getCurrentWorkingDirectory() << endl;
			string arguments = "make";
			for (auto arg: locals.args) {
				arguments += (" " + arg);
			}
			arguments += " -j";
			system(arguments.c_str());
			cout << endl;
		}
		else {
			cout << "matmake: could not find Matmakefile in " << files.getCurrentWorkingDirectory() << endl;
		}
		shouldQuit = true;
		return {shouldQuit, isError};
	}

	int lineNumber = 1;
	string line;

	auto getMultilineArgument = [&lineNumber, &matmakefile]() {
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
			else if (!locals.localOnly && words.size() >= 2 && words.front() == "external") {
				vout << "external dependency to " << words[1] << endl;

				environment.addExternalDependency(false,
												  words[1],
												  Tokens(words.begin() + 2, words.end()));
			}
			else if (!locals.localOnly && words.size() >= 2 && words.front() == "dependency") {
				environment.addExternalDependency(true,
												  words[1],
												  Tokens(words.begin() + 2, words.end()));
			}
		}
		++lineNumber;
	}

	return {shouldQuit, isError};
}


//! Do what operation was given in command line arguments and saved in locals
ShouldQuitT work(const Locals &locals, const Globals &globals, IEnvironment &environment) {
	try {
		if (locals.operation == "build") {
			environment.compile(locals.targets);
		}
		else if (locals.operation == "list") {
			environment.listAlternatives();
			return true;
		}
		else if (locals.operation == "clean") {
			environment.clean(locals.targets);
		}
		else if (locals.operation == "rebuild") {
			environment.clean(locals.targets);
			if (!globals.bailout) {
				environment.compile(locals.targets);
			}
		}
	}
	catch (MatmakeError &e) {
		cerr << e.what() << endl;
	}
	catch (std::runtime_error &e) {
		cerr << e.what() << endl;
	}

	return false;
}


//! The main entry point for a project.
//!
//! Runs once in the working directory and once in each directory specified
//! with the import keyword
int start(vector<string> args, Globals &globals) {
	auto startTime = time(nullptr);

	Locals locals;
	{
		ShouldQuitT shouldQuit;
		IsErrorT isError;
		std::tie(locals, shouldQuit, isError) = parseArguments(args, globals);

		if (isError) {
			return -1;
		}

		if (shouldQuit) {
			return 0;
		}
	}

	Environment environment(globals, make_shared<Files>());

	{
		ShouldQuitT shouldQuit;
		IsErrorT isError;
		std::tie(shouldQuit, isError) = parseMatmakeFile(locals, globals, environment);

		if (shouldQuit) {
			return 0;
		}
		if (isError) {
			globals.bailout = true;
		}
	}

	if (!globals.bailout) {
		if (work(locals, globals, environment)) {
			return 0;
		}
	}

	auto endTime = time(nullptr);

	auto duration = endTime - startTime;
	auto m = duration / 60;
	auto s = duration - m * 60;

	cout << endl;
	if (globals.bailout) {
		cout << "failed... " << m << "m " << s << "s" << endl;
		return -1;
	}
	else {
		string doneMessage = "done...";
		if (locals.operation == "clean") {
			doneMessage = "cleaned...";
		}
		cout << doneMessage << " " << m << "m " << s << "s" << endl;
		return 0;
	}
}

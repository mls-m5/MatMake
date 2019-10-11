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
	return -1;
}


int start(vector<string> args, Globals &globals) {
	auto startTime = time(nullptr);
	ifstream matmakefile("Matmakefile");

	string line;
	Environment environment(globals, new Files());
	auto& files = environment.fileHandler();

	auto env = [&] () -> IEnvironment& { return environment; }; // For dout and vout

	vector<string> targets;

	string operation = "build";
	bool localOnly = false;

	auto argc = args.size(); //To support old code
	for (size_t i = 0; i < args.size(); ++i) {
		string arg = args[i];
		if (arg == "clean") {
			operation = "clean";
		}
		else if (arg == "rebuild") {
			operation = "rebuild";
		}
		else if(arg == "--local") {
			localOnly = true;
		}
		else if (arg == "-v" || arg == "--verbose") {
			globals.verbose = true;
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
			globals.debugOutput = true;
			globals.verbose = true;
		}
		else if (arg == "-j") {
			++i;
			if (i < argc) {
				globals.numberOfThreads = static_cast<unsigned>(atoi(args[i].c_str()));
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
			cout << "makefile in " << files.getCurrentWorkingDirectory() << endl;
			string arguments = "make";
			for (auto arg: args) {
				arguments += (" " + arg);
			}
			arguments += " -j";
			system(arguments.c_str());
			cout << endl;
		}
		else {
			cout << "matmake: could not find Matmakefile in " << files.getCurrentWorkingDirectory() << endl;
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

				auto currentDirectory = files.getCurrentWorkingDirectory();

				if (!chdir(words[1].c_str())) {
					vector <string> newArgs(words.begin() + 2, words.end());

					if (operation == "clean") {
						start(args, globals);
					}
					else {
						start(newArgs, globals);
						if (globals.bailout) {
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

	if (!globals.bailout) {
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
			else if (operation == "rebuild") {
				environment.clean(targets);
				if (!globals.bailout) {
					environment.compile(targets);
				}
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
	if (globals.bailout) {
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
	Globals globals;
	vector<string> args(argv + 1, argv + argc);
	start(args, globals);
}


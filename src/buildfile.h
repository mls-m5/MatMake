// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "dependency.h"
#include "ibuildtarget.h"
#include "mdebug.h"
#include "globals.h"

#include <fstream>

//! Represent a single source file to be built
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
		  output(env->fileHandler().removeDoubleDots(
			  fixObjectEnding(parent->getBuildDirectory() + filename))),
		  depFile(env->fileHandler().removeDoubleDots(
			  fixDepEnding(parent->getBuildDirectory() + filename))),
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
		ifstream file(_parent->preprocessCommand(depFile));
		if (file.is_open()) {
			vector <string> ret;
			string d;
			file >> d; //The first is the target path
			while (file >> d) {
				if (d != "\\") { // Skip backslash (is used before newlines)
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

	Token preprocessCommand(Token command) {
		return _parent->preprocessCommand(command);
	}


	Token getFlags() {
		return _parent->getBuildFlags(filetype);
	}

	time_t build() override {
		auto flags = getFlags();

		auto depChangedTime = getDepFileChangedTime();
		auto inputChangedTime = getInputChangedTime();
		auto timeChanged = getTimeChanged();
		auto dependencyFiles = parseDepFile();

		if (   depChangedTime == 0
			|| inputChangedTime > depChangedTime
			|| inputChangedTime > timeChanged
			|| dependencyFiles.empty()) {
			dout << "file is dirty" << endl;
			depCommand = " -MMD -MF " + depFile + " ";
			depCommand.location = filename.location;
			dirty(true);
		}
		else {
			for (auto &d: dependencyFiles) {
				auto dependencyTimeChanged = env().fileHandler().getTimeChanged(d);
				if (   dependencyTimeChanged == 0
					|| dependencyTimeChanged > timeChanged) {
					dout << "rebuilding because older than " << d;
					dirty(true);
					break;
				}
			}
		}

		if (dirty()) {
			command = _parent->getCompiler(filetype) + " -c -o " + output
					  + " " + filename + " " + flags + depCommand;

			command = preprocessCommand(command);
			command.location = filename.location;
			queue(true);

			return time(nullptr);
		}
		else {
			return getTimeChanged();
		}
	}

	void work() override {
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


	BuildType buildType() override {
		return Object;
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

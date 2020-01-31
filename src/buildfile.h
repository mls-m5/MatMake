// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "dependency.h"
#include "ibuildtarget.h"
#include "mdebug.h"
#include "globals.h"

#include <fstream>

//! Represent a single source file to be built
class BuildFile: public Dependency {
	Token _filename; //The source of the file
	Token _filetype; //The ending of the filename

	IBuildTarget *_parent;

public:
	BuildFile(const BuildFile &) = delete;
	BuildFile(BuildFile &&) = delete;
	BuildFile(Token filename, IBuildTarget *parent, class IEnvironment *env):
		  Dependency(env),
		  _filename(filename),
		  _filetype(stripFileEnding(filename).second),
		  _parent(parent) {
		auto withoutEnding = stripFileEnding(parent->getBuildDirectory() + filename);
		if (withoutEnding.first.empty()) {
			throw MatmakeError(filename, "could not figure out source file type '" + parent->getBuildDirectory() + filename +
											 "' . Is the file ending right?");
		}

		Dependency::output(env->fileHandler().removeDoubleDots(
					  fixObjectEnding(parent->getBuildDirectory() + filename)));
		depFile(env->fileHandler().removeDoubleDots(
					  fixDepEnding(parent->getBuildDirectory() + filename)));
		if (filename.empty()) {
			throw MatmakeError(filename, "empty buildfile added");
		}

		if (output().empty()) {
			throw MatmakeError(filename, "could not find target name");
		}

		if (depFile().empty()) {
			throw MatmakeError(filename, "could not find dep filename for " + output());
		}
	}

	static Token fixObjectEnding(Token filename) {
		return stripFileEnding(filename).first + ".o";
	}
	static Token fixDepEnding(Token filename) {
		return filename + ".d";
	}

	time_t getInputChangedTime() {
		return env().fileHandler().getTimeChanged(_filename);
	}

	Token preprocessCommand(Token command) const {
		return _parent->preprocessCommand(command);
	}

	Token getFlags() {
		return _parent->getBuildFlags(_filetype);
	}

	BuildType buildType() override {
		return Object;
	}

	Token linkString() override {
		return output();
	}

	//! Calculate dependencies from makefile styled .d files generated
	//! by the compiler
	vector<string> parseDepFile() const {
		ifstream file(_parent->preprocessCommand(depFile()));
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
			dout << "could not find .d file for " << output() << " --> " << depFile() << endl;
			return {};
		}
	}


	void build() override {
		auto inputChangedTime = getInputChangedTime();
		auto dependencyFiles = parseDepFile();
		time_t outputChangedTime = changedTime();

		if (outputChangedTime < inputChangedTime) {
			dirty(true);
			dout << "file is dirty" << endl;
		}

		Token depCommand;

		if (dependencyFiles.empty()) {
			dout << "file is dirty" << endl;
			depCommand = " -MMD -MF " + depFile() + " ";
			depCommand.location = _filename.location;
			dirty(true);
		}
		else {
			for (auto &d: dependencyFiles) {
				auto dependencyTimeChanged = env().fileHandler().getTimeChanged(d);
				if (   dependencyTimeChanged == 0
						|| dependencyTimeChanged > outputChangedTime) {
					dout << "rebuilding because older than " << d;
					dirty(true);
					break;
				}
			}
		}

		if (dirty()) {
			auto flags = getFlags();
			Token command = _parent->getCompiler(_filetype) + " -c -o " + output()
					  + " " + _filename + " " + flags + depCommand;

			command = preprocessCommand(command);
			command.location = _filename.location;
			this->command(command);
		}
	}
};

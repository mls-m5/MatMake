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
	Token filetype; //The ending of the filename

	Token depCommand;

	IBuildTarget *_parent;

public:
	BuildFile(const BuildFile &) = delete;
	BuildFile(BuildFile &&) = delete;
	BuildFile(Token filename, IBuildTarget *parent, class IEnvironment *env):
		  Dependency(env),
		  filename(filename),
		  filetype(stripFileEnding(filename).second),
		  _parent(parent) {
		auto withoutEnding = stripFileEnding(parent->getBuildDirectory() + filename);
		if (withoutEnding.first.empty()) {
			throw MatmakeError(filename, "could not figure out source file type '" + parent->getBuildDirectory() + filename +
											 "' . Is the file ending right?");
		}

		Dependency::output(env->fileHandler().removeDoubleDots(
					  fixObjectEnding(parent->getBuildDirectory() + filename)));
		Dependency::depFile(env->fileHandler().removeDoubleDots(
					  fixDepEnding(parent->getBuildDirectory() + filename)));
		if (filename.empty()) {
			throw MatmakeError(filename, "empty buildfile added");
		}
		if (Dependency::outputs().size() < 2) {
			if (output().empty()) {
				throw MatmakeError(filename, "could not find target name");
			}
			if (Dependency::outputs()[1].empty()) {
				throw MatmakeError(filename, "could not find dep filename");
			}
		}
		else {
			depFile(Dependency::outputs()[1]);
		}
	}

	Token output() const {
		if (!outputs().empty()) {
			return outputs().front();
		}
		else {
			return "";
		}
	}

//	Token depFile() const {
//		if (_outputs.size() < 2) {
//			return "";
//		}
//		else {
//			return _outputs[1];
//		}
//	}

	static Token fixObjectEnding(Token filename) {
		return stripFileEnding(filename).first + ".o";
	}
	static Token fixDepEnding(Token filename) {
		return stripFileEnding(filename).first + ".d";
	}

	time_t getInputChangedTime() {
		return env().fileHandler().getTimeChanged(filename);
	}

	Token preprocessCommand(Token command) const {
		return _parent->preprocessCommand(command);
	}


	Token getFlags() {
		return _parent->getBuildFlags(filetype);
	}

	BuildType buildType() override {
		return Object;
	}

	Token targetPath() const override {
		return output();
	}

	Token linkString() override {
		return targetPath();
	}

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
		auto flags = getFlags();

		auto inputChangedTime = getInputChangedTime();
		auto dependencyFiles = parseDepFile();
		time_t outputChangedTime = getChangedTime();

		if (outputChangedTime < inputChangedTime) {
			dirty(true);
			dout << "file is dirty" << endl;
		}

		if (!dirty()) {
			if (dependencyFiles.empty()) {
				dout << "file is dirty" << endl;
				depCommand = " -MMD -MF " + depFile() + " ";
				depCommand.location = filename.location;
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
		}

		if (dirty()) {
			Token command = _parent->getCompiler(filetype) + " -c -o " + output()
					  + " " + filename + " " + flags + depCommand;

			command = preprocessCommand(command);
			command.location = filename.location;
			this->command(command);
			queue(true);

//			return time(nullptr);
		}
//		else {
//			return getTimeChanged();
//		}
	}

	void work() override {
		if (!command().empty()) {
			vout << command() << endl;
			pair <int, string> res = env().fileHandler().popenWithResult(command());
			if (res.first) {
				throw MatmakeError(command(), "could not build object:\n" + command() + "\n" + res.second);
			}
			else if (!res.second.empty()) {
				cout << (command() + "\n" + res.second + "\n") << std::flush;
			}
			dirty(false);
			sendSubscribersNotice();
		}
	}

};

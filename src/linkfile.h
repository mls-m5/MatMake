/*
 * linkfile.h
 *
 *  Created on: 25 jan. 2020
 *      Author: Mattias Larsson Sköld
 */

#pragma once

#include "dependency.h"
#include "compilertype.h"

class LinkFile: public Dependency {
	bool _isBuildCalled = false;
	ICompiler *_compilerType;
	Token _command;

public:
	LinkFile(const LinkFile &) = delete;
	LinkFile(LinkFile &&) = delete;
	LinkFile(Token filename, IBuildTarget *target, class IEnvironment *env,
			ICompiler* compilerType) :
			Dependency(env, target),
			_compilerType(compilerType) {
		output(
				env->fileHandler().removeDoubleDots(
						target->getOutputDir() + filename));
	}


	time_t getTimeChanged() {
		return env().fileHandler().getTimeChanged(output());
	}

	void build() override {
		if (_isBuildCalled) {
			return;
		}
		_isBuildCalled = true;

		auto exe = output();
		if (exe.empty() || target()->name() == "root") {
			return;
		}

		dirty(false);


		time_t lastDependency = 0;
		for (auto &d: dependencies()) {
			auto t = changedTime();
			if (d->dirty()) {
				d->addSubscriber(this);
				std::lock_guard<Dependency> g(*this);
			}
			if (t > lastDependency) {
				lastDependency = t;
			}
			if (t == 0) {
				dirty(true);
			}
		}

		if (lastDependency > getTimeChanged()) {
			dirty(true);
		}
		else if (!dirty()) {
			cout << output() << " is fresh " << endl;
		}


		if (dirty()) {
			Token fileList;

			for (auto f: dependencies()) {
				if (f->includeInBinary()) {
					fileList += (f->linkString() + " ");
				}
			}

			auto cpp = target()->getCompiler("cpp");

			auto out = target()->get("out");
			auto buildType = target()->buildType();
			if (buildType == Shared) {
				_command = cpp + " -shared -o " + exe + " -Wl,--start-group "
						  + fileList + " " + target()->getLibs() + "  -Wl,--end-group  "
						  + target()->getFlags();
			}
			else if (buildType == Static) {
				_command = "ar -rs " + exe + " "  + fileList;
				if (env().globals().verbose) {
					_command += " -v ";
				}
			}
			else {
				_command = cpp + " -o " + exe + " -Wl,--start-group "
						  + fileList + " " + target()->getLibs() + "  -Wl,--end-group  "
						  + target()->getFlags();
			}
			_command = target()->preprocessCommand(_command);

			if (buildType == Executable || buildType == Shared) {
				if (hasReferencesToSharedLibrary()) {
					_command += (" " + _compilerType->getString(CompilerString::RPathOriginFlag));
				}
			}
			_command.location = cpp.location;
		}
	}

	//! This is called when all dependencies are built
	void work() override {
		vout << "linking " << target()->name() << endl;
		vout << _command << endl;
		pair <int, string> res = env().fileHandler().popenWithResult(_command);
		if (res.first) {
			throw MatmakeError(_command, "linking failed with\n" + _command + "\n" + res.second);
		}
		else if (!res.second.empty()) {
			cout << (_command + "\n" + res.second + "\n") << flush;
		}
		dirty(false);
		sendSubscribersNotice();
		vout << endl;
	}


	Token linkString() const override {
		auto dir = target()->getOutputDir();
		if (buildType() == Shared) {
			if (dir.empty()) {
				dir = ".";
			}
			return _compilerType->prepareLinkString(dir, target()->filename());
		}
		else {
			return output();
		}
	}

	BuildType buildType() const override {
		return target()->buildType();
	}


	//! This is to check if should include linker -rpath or similar
	bool hasReferencesToSharedLibrary() {
		for (auto &d: dependencies()) {
			if (d->buildType() == Shared) {
				return true;
			}
		}
		return false;
	}


};


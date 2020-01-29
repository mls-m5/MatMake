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
	IBuildTarget *_target; // The build target responsible for this file
//	Token _output; // The output filename
	bool _isBuildCalled = false;
	ICompiler *_compilerType;
	Token _command;

public:
	LinkFile(const LinkFile &) = delete;
	LinkFile(LinkFile &&) = delete;
	LinkFile(Token filename, IBuildTarget *target, class IEnvironment *env,
			ICompiler* compilerType) :
			Dependency(env),
			_target(target),
			_compilerType(compilerType) {
		output(env->fileHandler().removeDoubleDots(filename));
	}




//	virtual time_t getTimeChanged() override {
//		return env().fileHandler().getTimeChanged(
//				_target->preprocessCommand(targetPath()));
//	}


//
//	void notice(IDependency * d) override {
//		std::lock_guard<Dependency> g(*this);
//		waitList.erase(waitList.find(d));
//		dout << d->targetPath() << " removed from wating list from " << _name
//			 << " " << waitList.size() << " left" << endl;
//		if (waitList.empty()) {
//			queue(false);
//		}
//	}

	void dependenciesComplete() override {
		dout << output() << " is ready to be built" << endl;
		queue(false);
	}

	time_t getTimeChanged() {
		return env().fileHandler().getTimeChanged(output());
	}

	void build() override {
		if (_isBuildCalled) {
			return;
//			return getTimeChanged();
		}
		_isBuildCalled = true;

		auto exe = output();
		if (exe.empty() || _target->name() == "root") {
			return;
//			return 0;
		}

		dirty(false);


		time_t lastDependency = 0;
		for (auto &d: dependencies()) {
//			auto t = d->build();
			auto t = getChangedTime();
			if (d->dirty()) {
				d->addSubscriber(this);
				std::lock_guard<Dependency> g(*this);
//				waitList.insert(d);
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
			cout << output() << " is fresh " << endl;
		}


		if (dirty()) {
			Token fileList;

			for (auto f: dependencies()) {
				if (f->includeInBinary()) {
					fileList += (f->linkString() + " ");
				}
			}

			auto cpp = _target->getCompiler("cpp");

			auto out = _target->get("out");
			auto buildType = _target->buildType();
			if (buildType == Shared) {
				_command = cpp + " -shared -o " + exe + " -Wl,--start-group "
						  + fileList + " " + _target->getLibs() + "  -Wl,--end-group  "
						  + _target->getFlags();
			}
			else if (buildType == Static) {
				_command = "ar -rs " + exe + " "  + fileList;
				if (env().globals().verbose) {
					_command += " -v ";
				}
			}
			else {
				_command = cpp + " -o " + exe + " -Wl,--start-group "
						  + fileList + " " + _target->getLibs() + "  -Wl,--end-group  "
						  + _target->getFlags();
			}
			_command = _target->preprocessCommand(_command);

			if (buildType == Executable || buildType == Shared) {
				if (hasReferencesToSharedLibrary()) {
					_command += (" " + _compilerType->getString(CompilerString::RPathOriginFlag));
				}
			}
			_command.location = cpp.location;


			if (dependencies().empty()) {
				queue(true);
			}
			else {
				hintStatistic();
			}

//			return time(nullptr);
		}

//		return getTimeChanged();
	}

	//! This is called when all dependencies are built
	void work() override {
		vout << "linking " << _target->name() << endl;
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


	void clean() override {
		for (auto &d: dependencies()) {
			d->clean();
		}
		vout << "removing file " << output() << endl;
		remove(output().c_str());
	}


	Token linkString() override {
		auto dir = _target->getOutputDir();
		if (buildType() == Shared) {
			if (dir.empty()) {
				dir = ".";
			}
			return _compilerType->prepareLinkString(dir, _target->filename());
		}
		else {
			return output();
		}
	}

	BuildType buildType() override {
		return _target->buildType();
	}


//	Token output() const override {
//		auto dir = _target->getOutputDir();
//		return dir + _target->filename();
//	}


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


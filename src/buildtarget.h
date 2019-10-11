// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "ibuildtarget.h"
#include "dependency.h"

#include "globals.h"

#include <map>

//! A build target is a executable, dll or similar that depends
//! on one or more build targets, build files or copy files
struct BuildTarget: public Dependency, public IBuildTarget {
	std::map<Token, Tokens> _properties;
	Token name;
	set<IDependency *> waitList;
	Token command;

	BuildTarget(Token name, class IEnvironment *env): Dependency(env), name(name) {
		if (name != "root") {
			assign("inherit", Token("root"));
		}
		else {
			assign("cpp", Token("c++"));
			assign("cc", Token("cc"));
		}
	}

	BuildTarget(class IEnvironment *env): Dependency(env){
		assign("inherit", Token("root"));
	}

	BuildTarget(NameDescriptor n, Tokens v, IEnvironment *env): BuildTarget(n.rootName, env) {
		assign(n.propertyName, v);
	}

	void inherit(const IBuildTarget &parent) {
		for (auto v: parent.properties()) {
			if (v.first == "inherit") {
				continue;
			}
			assign(v.first, v.second);
		}
	}


	Tokens &property(Token propertyName) override {
		return _properties[propertyName];
	}

	void assign(Token propertyName, Tokens value) override {
		property(propertyName) = value;

		if (propertyName == "inherit") {
			auto parent = getParent();
			if (parent) {
				inherit(*parent);
			}
		}
		else if (propertyName == "exe" || propertyName == "dll") {
			if (trim(value.concat()) == "%") {
				property(propertyName) = name;
			}
		}

		if (propertyName == "dll") {
			auto n = property(propertyName);
			if (!n.empty()) {
				auto ending = stripFileEnding(n.concat(), true);
				if (!ending.second.empty()) {
					property(propertyName) = Token(ending.first + ".so");
				}
				else {
					property(propertyName) = Token(n.concat() + ".so");
				}
			}
		}
	}

	void append(Token propertyName, Tokens value) override {
		property(propertyName).append(value);
	}

	Tokens get(const Token &propertyName) override {
		try {
			return properties().at(propertyName);
		}
		catch (out_of_range &) {
			return {};
		}
	}

	const map<Token, Tokens> &properties() const override {
		return _properties;
	}

	//! Returns all files in a property
	Tokens getGroups(const Token &propertyName) {
		auto sourceString = get(propertyName);

		auto groups = sourceString.groups();

		Tokens ret;
		for (auto g: groups) {
			auto sourceFiles = environment()->fileHandler().findFiles(g.concat());
			ret.insert(ret.end(), sourceFiles.begin(), sourceFiles.end());
		}

		if (ret.size() == 1) {
			if (ret.front().empty()) {
				vout << " no pattern matching for " << propertyName << endl;
			}
		}
		return ret;
	}

	//! Get the parent inherited from
	//! This differs from the _parent member
	//! #TODO: Fix naming
	IBuildTarget *getParent()  {
		auto inheritFrom = get("inherit").concat();
		return environment()->findTarget(inheritFrom);
	}

	void print() override {
		vout << "target " << name << ": " << endl;
		for (auto &m: properties()) {
			vout << "\t" << m.first << " = " << m.second << " " << endl;
		}
		vout << endl;
	}

	Token getCompiler(Token filetype) override {
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
		for (auto &d: dependencies()) {
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

	void notice(IDependency * d) override {
		{
			lock_guard<Dependency> g(*this);
			waitList.erase(waitList.find(d));
		}
		dout << d->targetPath() << " removed from wating list from " << name << " " << waitList.size() << " left" << endl;
		if (waitList.empty()) {
			queue(false);
		}
	}

	//! This is called when all dependencies are built
	void work() override {
		vout << "linking " << name << endl;
		vout << command << endl;
		pair <int, string> res = env().fileHandler().popenWithResult(command);
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
		auto dir = getOutputDir();
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
		auto outputDir = get("dir").concat();
		if (!outputDir.empty()) {
			outputDir = outputDir.trim();
			outputDir += "/";
		}
		return outputDir;
	}

	//!If where the tmp build-files is placed
	Token getBuildDirectory() override {
		auto outputDir = get("objdir").concat();
		if (!outputDir.empty()) {
			outputDir = outputDir.trim();
			outputDir += "/";
		}
		else {
			return getOutputDir();
		}
		return outputDir;
	}
};

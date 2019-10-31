// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "ibuildtarget.h"
#include "dependency.h"
#include "buildfile.h"
#include "copyfile.h"

#include "globals.h"
#include "compilertype.h"
#include "ifiles.h"

#include <map>


//! A build target is a executable, dll or similar that depends
//! on one or more build targets, build files or copy files
struct BuildTarget: public Dependency, public IBuildTarget {
	std::map<Token, Tokens> _properties;
	Token _name;
	set<IDependency *> waitList;
	Token command;
	Token buildFlags;
	shared_ptr<ICompiler> compilerType = make_shared<GCCCompiler>();
	bool _isBuildCalled = false;

	BuildTarget(Token name, class IEnvironment *env): Dependency(env), _name(name) {
		if (_name != "root") {
			assign("inherit", Token("root"));
		}
		else {
			assign("cpp", Token("c++"));
			assign("cc", Token("cc"));
			assign("includes", Token(""));
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

		if (propertyName == "dll") {
			auto n = property(propertyName);
			cout << "dll property is deprecated, plese use .out = shared " << n.concat() << endl;
			value.insert(value.begin(), Token("shared "));
			property("out") = value;
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


	BuildType buildType() override {
		auto out = get("out");
		if (!out.empty()) {
			if (out.front() == "shared") {
				return Shared;
			}
			else if (out.front() == "static") {
				return Static;
			}
		}
		return Executable;
	}

	Token preprocessCommand(Token command) override {
		for (size_t find = command.find('%');
			 find != string::npos;
			 find = command.find('%', find + 1)) {
			command.replace(find, 1, name());
		}

		return command;
	}

	Token getIncludeFlags() {
		Token ret;

		auto includes = get("includes").groups();
		for (auto &include: includes) {
			auto includeStr = include.concat();
			if (includeStr.empty()) {
				continue;
			}
			ret += (" "
					+ compilerType->getString(CompilerString::IncludePrefix)
					+ includeStr);
		}

		auto sysincludes = get("sysincludes").groups();
		for (auto &include: sysincludes) {
			auto includeStr = include.concat().trim();
			if (includeStr.empty()) {
				continue;
			}
			ret += (" "
					+ compilerType->getString(CompilerString::SystemIncludePrefix)
					+ include.concat());
		}

		return ret;
	}

	Token getDefineFlags() {
		Token ret;

		auto defines = get("define").groups();
		for (auto &def: defines) {
			ret += (" "
					+ compilerType->getString(CompilerString::DefinePrefix)
					+ def.concat());
		}


		return ret;
	}

	Token getConfigFlags() {
		Token ret;
		auto configs = get("config").groups();
		for (auto &config: configs) {
			ret += (" " + compilerType->translateConfig(config.concat()));
		}
		return ret;
	}

	Token getLibs() {
		auto libs = get("libs");

		return libs.concat();
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

	//! Return flags used by a file
	virtual Token getBuildFlags(const Token& filetype) override {
		if (!buildFlags.empty()) {
			return buildFlags;
		}

		auto flags = get("flags").concat();
		if (filetype == "cpp") {
			auto cppflags = get("cppflags");
			if (!cppflags.empty()) {
				flags += (" " + cppflags.concat());
			}
		}
		if (filetype == "c") {
			auto cflags = get("cflags");
			if (!cflags.empty()) {
				flags += (" " + cflags.concat());
			}
		}

		flags += getDefineFlags();

		flags += getConfigFlags();

		flags += getIncludeFlags();
		auto buildType = this->buildType();
		if (buildType == Shared) {
			if (compilerType->getFlag(CompilerFlagType::RequiresPICForLibrary)) {
				flags += (" " + compilerType->getString(CompilerString::PICFlag));
			}
		}


		return (buildFlags = flags);
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
		vout << "target " << _name << ": " << endl;
		for (auto &m: properties()) {
			vout << "\t" << m.first << " = " << m.second << " " << endl;
		}
		vout << endl;
	}

	Token getCompiler(const Token &filetype) override {
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

	virtual time_t getTimeChanged() override {
		return env().fileHandler().getTimeChanged(preprocessCommand(targetPath()));
	}

	time_t build() override {
		if (_isBuildCalled) {
			return getTimeChanged();
		}
		_isBuildCalled = true;

		dirty(false);
		auto exe = targetPath();
		if (exe.empty() || _name == "root") {
			return 0;
		}

		vout << endl;
		vout << "  target " << _name << "..." << endl;

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
			cout << _name << " is fresh " << endl;
		}

		if (dirty()) {
			Token fileList;

			for (auto f: dependencies()) {
				if (f->includeInBinary()) {
					fileList += (f->linkString() + " ");
				}
			}

			auto cpp = getCompiler("cpp");

			auto out = get("out");
			auto buildType = this->buildType();
			if (buildType == Shared) {
				command = cpp + " -shared -o " + exe + " -Wl,--start-group "
						  + fileList + " " + getLibs() + "  -Wl,--end-group  "
						  + get("flags").concat();
			}
			else if (buildType == Static) {
				command = "ar -rs " + exe + " "  + fileList;
				if (env().globals().verbose) {
					command += " -v ";
				}
			}
			else {
				command = cpp + " -o " + exe + " -Wl,--start-group "
						  + fileList + " " + getLibs() + "  -Wl,--end-group  "
						  + get("flags").concat();
			}
			command = preprocessCommand(command);

			if (buildType == Executable || buildType == Shared) {
				if (hasReferencesToSharedLibrary()) {
					command += (" " + compilerType->getString(CompilerString::RPathOriginFlag));
				}
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
		lock_guard<Dependency> g(*this);
		waitList.erase(waitList.find(d));
		dout << d->targetPath() << " removed from wating list from " << _name
			 << " " << waitList.size() << " left" << endl;
		if (waitList.empty()) {
			queue(false);
		}
	}

	//! This is called when all dependencies are built
	void work() override {
		vout << "linking " << _name << endl;
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

	//! Path minus directory
	Token filename() {
		auto out = get("out").groups();
		if (out.empty()) {
			return _name;
		}
		else if (out.size() == 1) {
			return preprocessCommand(out.front().concat());
		}
		else if (out.size() > 1) {
			auto type = out.front().concat();
			auto striped = stripFileEnding(preprocessCommand(out[1].concat()),
										   true);
			auto outName = striped.first;

			if (type == "shared") {
				return outName + compilerType->getString(
						   CompilerString::SharedFileEnding);
			}
			else if (type == "static") {
				return outName + compilerType->getString(
						   CompilerString::StaticFileEnding);
			}
			else if (type == "exe") {
				return outName;
			}
			else {
				throw MatmakeError(type, "Unknown type " + type);
			}
		}
		throw std::runtime_error("this should never happend");
	}

	Token linkString() override {
		auto dir = getOutputDir();
		if (buildType() == Shared) {
			if (dir.empty()) {
				dir = ".";
			}
			return " " + filename() + " -L" + dir;
		}
		else {
			return targetPath();
		}
	}

	Token targetPath() override {
		auto dir = getOutputDir();
		return dir + filename();
	}

	Token name() override {
		return _name;
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

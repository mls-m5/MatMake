// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "ienvironment.h"

#include "globals.h"
#include "token.h"
#include "ibuildtarget.h"
#include "targets.h"
#include "threadpool.h"

#include <fstream>

class Environment: public IEnvironment {
public:
	struct ExternalMatmakeType {
		bool compileBefore;
		Token name;
		Tokens arguments;
	};

	Targets targets;
	ThreadPool tasks;
	std::shared_ptr<IFiles> _fileHandler;
	std::vector<ExternalMatmakeType> externalDependencies;

	void addExternalDependency(bool shouldCompileBefore,
							   const Token &name,
							   const Tokens &args) override {
		externalDependencies.push_back({shouldCompileBefore,
										name,
										args});
	}

	void buildExternal(bool isBefore, string operation) {
		for (auto &dependency: externalDependencies) {
			if (dependency.compileBefore != isBefore) {
				continue;
			}

			auto currentDirectory = _fileHandler->getCurrentWorkingDirectory();

			if (!_fileHandler->setCurrentDirectory(dependency.name)) {
				vector <string> newArgs(dependency.arguments.begin(),
									   dependency.arguments.end());

				if (operation == "clean") {
					cout << endl << "cleaning external " << dependency.name << endl;
					start({"clean"});
				}
				else {
					cout << endl << "Building external " << dependency.name << endl;
					start(newArgs);
					if (globals.bailout) {
						break;
					}
					cout << endl;
				}
			}
			else {
				throw runtime_error("could not external directory " + dependency.name);
			}

			if (_fileHandler->setCurrentDirectory(currentDirectory)) {
				throw runtime_error("could not go back to original working directory " + currentDirectory);
			}
			else {
				vout << "returning to " << currentDirectory << endl;
				vout << endl;
			}
		}
	}


	Environment (shared_ptr<IFiles> fileHandler):
		  _fileHandler(fileHandler)
	{
		targets.emplace_back(new BuildTarget("root", nullptr));
		targets.root = targets.back().get();
	}


	IBuildTarget *findVariable(Token name) const {
		if (name.empty()) {
			return nullptr;
		}
		for (auto &v: targets) {
			if (v->name() == name) {
				return v.get();
			}
		}
		return nullptr;
	}

	IBuildTarget &operator[] (Token name) {
		if (auto v = findVariable(name)) {
			return *v;
		}
		else {
			targets.emplace_back(new BuildTarget(name, targets.root));
			return *targets.back();
		}
	}

	void appendVariable(const NameDescriptor &name, Tokens value) override {
		(*this) [name.rootName].append(name.propertyName, value);
	}

	void setVariable(const NameDescriptor &name, Tokens value) override {
		(*this) [name.rootName].assign(name.propertyName, value, targets) ;
	}

	void print() {
		for (auto &v: targets) {
			v->print();
		}
	}

	vector<IBuildTarget*> parseTargetArguments(vector<string> targetArguments) const {
		vector<IBuildTarget*> selectedTargets;

		if (targetArguments.empty()) {
			for (auto& target: targets) {
				selectedTargets.push_back(target.get());
			}
		}
		else {
			for (auto t: targetArguments) {
				bool matchFailed = true;
				auto target = targets.find(t);
				if (target) {

					selectedTargets.push_back(target);
					matchFailed = false;
				}
				else {
					cout << "target '" << t << "' does not exist" << endl;
				}

				if (matchFailed) {
					cout << "run matmake --help for help" << endl;
					cout << "targets: ";
					listAlternatives();
					throw runtime_error("error when parsing targets");
				}
			}
		}

		return selectedTargets;
	}

	vector<unique_ptr<IDependency>> calculateDependencies(
			vector<IBuildTarget*> selectedTargets) const {
		vector < unique_ptr < IDependency >> files;
		for (auto target : selectedTargets) {
			dout << "target " << target->name() << " src "
					<< target->get("src").concat() << endl;

			auto targetDependencies = target->calculateDependencies(*_fileHandler, targets);

			typedef decltype (files)::iterator iter_t;

			files.insert(files.end(),
					std::move_iterator<iter_t>(targetDependencies.begin()),
					std::move_iterator<iter_t>(targetDependencies.end()));

		}

		return files;
	}

	void createDirectories(const vector<unique_ptr<IDependency>>& files) const {
		set<string> directories;
		for (auto &file: files) {
			if (!file->dirty()) {
				continue;
			}
			auto dir = _fileHandler->getDirectory(file->output());
			if (!dir.empty()) {
				directories.emplace(dir);
			}
		}

		for (auto dir: directories) {
			dout << "output dir: " << dir << endl;
			if (dir.empty()) {
				continue;
			}
			if (_fileHandler->isDirectory(dir)) {
				continue; //Skip if already created
			}
			_fileHandler->createDirectory(dir);
		}
	}

	void compile(vector<string> targetArguments) override {
		dout << "compiling..." << endl;
		print();

		auto files = calculateDependencies(parseTargetArguments(targetArguments));

		for (auto& file: files) {
			file->build(*_fileHandler);
		}

		createDirectories(files);

		for (auto &file: files) {
			if (file->dirty()) {
				dout << "file " << file->output() << " is dirty" << endl;
				tasks.addTaskCount();
				file->prune();
				if (file->dependencies().empty()) {
					tasks.addTask(file.get());
				}
			}
			else {
				dout << "file " << file->output() << " is fresh" << endl;
			}
		}

		buildExternal(true, "");
		work(std::move(files));
		buildExternal(false, "");
	}

	void work(vector<unique_ptr<IDependency>> files) {
		tasks.work(std::move(files), *_fileHandler);
	}


	void clean(vector<string> targetArguments) override {
		dout << "cleaning " << endl;
		auto files = calculateDependencies(parseTargetArguments(targetArguments));

		if (targetArguments.empty()) {
			buildExternal(true, "clean");

			for (auto& file: files) {
				file->clean(*_fileHandler);
			}

			buildExternal(false, "clean");
		}
		else {
			for (auto& file: files) {
				file->clean(*_fileHandler);
			}
		}

	}

	//! Show info of alternative build targets
	void listAlternatives() const override {
		for (auto &t: targets) {
			if (t->name() != "root") {
				cout << t->name() << " ";
			}
		}
		cout << "clean";
		cout << endl;
	}
};






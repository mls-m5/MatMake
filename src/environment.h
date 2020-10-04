// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "ienvironment.h"

#include "buildtarget.h"
#include "globals.h"
#include "ibuildtarget.h"
#include "matmake.h"
#include "targets.h"
#include "threadpool.h"
#include "token.h"

#include <fstream>

class Environment : public IEnvironment {
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
        externalDependencies.push_back({shouldCompileBefore, name, args});
    }

    void buildExternal(bool isBefore, std::string operation) {
        using namespace std;
        for (auto &dependency : externalDependencies) {
            if (dependency.compileBefore != isBefore) {
                continue;
            }

            auto currentDirectory = _fileHandler->getCurrentWorkingDirectory();

            if (!_fileHandler->setCurrentDirectory(dependency.name)) {
                vector<string> newArgs(dependency.arguments.begin(),
                                       dependency.arguments.end());

                if (operation == "clean") {
                    cout << endl
                         << "cleaning external " << dependency.name << endl;
                    start({"clean"});
                }
                else {
                    cout << endl
                         << "Building external " << dependency.name << endl;
                    start(newArgs);
                    if (globals.bailout) {
                        break;
                    }
                    cout << endl;
                }
            }
            else {
                throw runtime_error("could not external directory " +
                                    dependency.name);
            }

            if (_fileHandler->setCurrentDirectory(currentDirectory)) {
                throw runtime_error(
                    "could not go back to original working directory " +
                    currentDirectory);
            }
            else {
                vout << "returning to " << currentDirectory << endl;
                vout << endl;
            }
        }
    }

    void setCommandLineVars(
        const std::map<std::string, std::vector<std::string>> &vars) override {
        for (auto element : vars) {
            for (auto var : element.second) {
                targets.root->append(element.first, Token(var));
            }
        }
    }

    Environment(std::shared_ptr<IFiles> fileHandler)
        : _fileHandler(fileHandler) {
        targets.emplace_back(new BuildTarget("root", nullptr));
        targets.root = targets.back().get();
    }

    IBuildTarget *findVariable(Token name) const {
        if (name.empty()) {
            return nullptr;
        }
        for (auto &v : targets) {
            if (v->name() == name) {
                return v.get();
            }
        }
        return nullptr;
    }

    IBuildTarget &operator[](Token name) {
        if (auto v = findVariable(name)) {
            return *v;
        }
        else {
            targets.emplace_back(new BuildTarget(name, targets.root));
            return *targets.back();
        }
    }

    void appendVariable(const NameDescriptor &name, Tokens value) override {
        (*this)[name.rootName].append(name.propertyName, value);
    }

    void setVariable(const NameDescriptor &name, Tokens value) override {
        (*this)[name.rootName].assign(name.propertyName, value, targets);
    }

    void print() {
        for (auto &v : targets) {
            v->print();
        }
    }

    std::vector<IBuildTarget *> parseTargetArguments(
        std::vector<std::string> targetArguments) const {
        std::vector<IBuildTarget *> selectedTargets;

        if (targetArguments.empty()) {
            for (auto &target : targets) {
                selectedTargets.push_back(target.get());
            }
        }
        else {
            for (auto t : targetArguments) {
                bool matchFailed = true;
                auto target = targets.find(t);
                if (target) {

                    selectedTargets.push_back(target);
                    matchFailed = false;
                }
                else {
                    std::cout << "target '" << t << "' does not exist\n";
                }

                if (matchFailed) {
                    std::cout << "run matmake --help for help\n";
                    std::cout << "targets: ";
                    listAlternatives();
                    throw std::runtime_error("error when parsing targets");
                }
            }
        }

        return selectedTargets;
    }

    std::vector<std::unique_ptr<IDependency>> calculateDependencies(
        std::vector<IBuildTarget *> selectedTargets) const {
        std::vector<std::unique_ptr<IDependency>> files;
        for (auto target : selectedTargets) {
            dout << "target " << target->name() << " src "
                 << target->get("src").concat() << std::endl;

            auto targetDependencies =
                target->calculateDependencies(*_fileHandler, targets);

            typedef decltype(files)::iterator iter_t;

            files.insert(files.end(),
                         std::move_iterator<iter_t>(targetDependencies.begin()),
                         std::move_iterator<iter_t>(targetDependencies.end()));
        }

        return files;
    }

    void createDirectories(
        const std::vector<std::unique_ptr<IDependency>> &files) const {
        std::set<std::string> directories;
        for (auto &file : files) {
            if (!file->dirty()) {
                continue;
            }
            auto dir = _fileHandler->getDirectory(file->output());
            if (!dir.empty()) {
                directories.emplace(dir);
            }
        }

        for (auto dir : directories) {
            dout << "output dir: " << dir << std::endl;
            if (dir.empty()) {
                continue;
            }
            if (_fileHandler->isDirectory(dir)) {
                continue; // Skip if already created
            }
            _fileHandler->createDirectory(dir);
        }
    }

    void compile(std::vector<std::string> targetArguments) override {
        dout << "compiling..." << std::endl;
        print();

        auto files =
            calculateDependencies(parseTargetArguments(targetArguments));

        for (auto &file : files) {
            file->prepare(*_fileHandler);
        }

        createDirectories(files);

        for (auto &file : files) {
            file->prune();
            if (file->dirty()) {
                dout << "file " << file->output() << " is dirty" << std::endl;
                tasks.addTaskCount();
                if (file->dependencies().empty()) {
                    tasks.addTask(file.get());
                }
            }
            else {
                dout << "file " << file->output() << " is fresh" << std::endl;
            }
        }

        buildExternal(true, "");
        work(std::move(files));
        buildExternal(false, "");
    }

    void work(std::vector<std::unique_ptr<IDependency>> files) {
        tasks.work(std::move(files), *_fileHandler);
    }

    void clean(std::vector<std::string> targetArguments) override {
        dout << "cleaning " << std::endl;
        auto files =
            calculateDependencies(parseTargetArguments(targetArguments));

        if (targetArguments.empty()) {
            buildExternal(true, "clean");

            for (auto &file : files) {
                file->clean(*_fileHandler);
            }

            buildExternal(false, "clean");
        }
        else {
            for (auto &file : files) {
                file->clean(*_fileHandler);
            }
        }
    }

    //! Show info of alternative build targets
    void listAlternatives() const override {
        for (auto &t : targets) {
            if (t->name() != "root") {
                std::cout << t->name() << " ";
            }
        }
        std::cout << "clean\n";
    }
};

/*
 * linkfile.h
 *
 *  Created on: 25 jan. 2020
 *      Author: Mattias Larsson Sköld
 */

#pragma once

#include "compilertype.h"
#include "dependency/dependency.h"

class LinkFile : public Dependency {

public:
    LinkFile(const LinkFile &) = delete;
    LinkFile(LinkFile &&) = delete;
    LinkFile(Token filename, IBuildTarget *target, ICompiler *compilerType)
        : Dependency(target)
        , _compilerType(compilerType) {
        output(removeDoubleDots(target->getOutputDir() + filename));

        depFile(removeDoubleDots(
            fixDepEnding(target->getBuildDirectory() + filename)));
    }

    void prescan(IFiles &,
                 const std::vector<std::unique_ptr<IDependency>> &) override {}

    void prepare(const IFiles &files) override {
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
        for (auto &d : dependencies()) {
            auto t = d->changedTime(files);
            if (d->dirty()) {
                d->addSubscriber(this);
                dirty(true);
            }
            lastDependency = std::max(t, lastDependency);
            if (t == 0) {
                dirty(true);
            }
        }

        if (lastDependency > changedTime(files)) {
            dirty(true);
        }
        else if (!dirty()) {
            std::cout << output() << " is fresh " << std::endl;
        }

        prepareCommand();

        std::vector<std::string> oldDependencies;
        std::string oldCommand;
        tie(oldDependencies, oldCommand) = parseDepFile();
        if (command() != oldCommand) {
            dout << output() << " command differs \n";
            dout << command() << "\n";
            dout << oldCommand << "\n\n";
            dirty(true);
        }
    }

    std::string work(const IFiles &files, ThreadPool &pool) override {
        if (!command().empty()) {
            {
                files.replaceFile(depFile(), _dependencyString);
                std::ofstream file(depFile());
                file << _dependencyString;
            }
            return Dependency::work(files, pool);
        }

        return {};
    }

private:
    void prepareCommand() {
        Token fileList;

        for (auto f : dependencies()) {
            if (f->includeInBinary()) {
                fileList += (f->linkString() + " ");
            }
        }

        _dependencyString = prepareDependencyString();

        auto cpp = target()->getCompiler("cpp");

        auto exe = output();
        auto buildType = target()->buildType();
        Token cmd;
        if (buildType == Shared) {
            cmd = cpp + " -shared -o " + exe + " -Wl,--start-group " +
                  fileList + " " + target()->getLibs() + "  -Wl,--end-group  " +
                  target()->getFlags();
        }
        else if (buildType == Static) {
            cmd = "ar -rs " + exe + " " + fileList;
            if (globals.verbose) {
                cmd += " -v ";
            }
        }
        else {
            cmd = cpp + " -o " + exe + " -Wl,--start-group " + fileList + " " +
                  target()->getLibs() + "  -Wl,--end-group  " +
                  target()->getFlags();
        }
        cmd = target()->preprocessCommand(cmd);

        if (buildType == Executable || buildType == Shared) {
            if (hasReferencesToSharedLibrary()) {
                cmd += (" " + _compilerType->getString(
                                  CompilerString::RPathOriginFlag));
            }
        }
        cmd.location = cpp.location;
        while (!cmd.empty() && isspace(cmd.back())) {
            cmd.pop_back();
        }

        command(cmd);
    }

    std::string prepareDependencyString() const {
        std::ostringstream ss;
        ss << output() << ":";
        for (auto &d : dependencies()) {
            ss << " " << d->output();
        }
        ss << "\n";
        return ss.str();
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
    bool hasReferencesToSharedLibrary() const {
        for (auto &d : dependencies()) {
            if (d->buildType() == Shared) {
                return true;
            }
        }
        return false;
    }

    bool _isBuildCalled = false;
    ICompiler *_compilerType;
    std::string _dependencyString;
};

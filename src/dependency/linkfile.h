/*
 * linkfile.h
 *
 *  Created on: 25 jan. 2020
 *      Author: Mattias Larsson Sköld
 */

#pragma once

#include "compilertype.h"
#include "dependency/dependency.h"
#include "dependency/ibuildrule.h"

class LinkFile : public IBuildRule {

public:
    LinkFile(const LinkFile &) = delete;
    LinkFile(LinkFile &&) = delete;
    LinkFile(Token filename,
             IBuildTarget *target,
             ICompiler *compilerType,
             std::unique_ptr<IDependency> dependency = nullptr)
        : _dep(dependency ? std::move(dependency)
                          : std::make_unique<Dependency>(
                                target, true, FromTarget, this))
        , _compilerType(compilerType) {
        _dep->output(removeDoubleDots(target->getOutputDir() + filename));

        _dep->depFile(removeDoubleDots(Dependency::fixDepEnding(
            removeDoubleDots(target->getBuildDirectory()) + filename)));

        auto dir = _dep->target()->getOutputDir();
        if (_dep->buildType() == Shared) {
            if (dir.empty()) {
                dir = ".";
            }
            _dep->linkString(_compilerType->prepareLinkString(
                dir, _dep->target()->filename()));
        }
        else {
            _dep->linkString(_dep->output());
        }
    }

    void prescan(IFiles &, const BuildRuleList &) override {}

    void prepare(const IFiles &files, BuildRuleList &) override {
        if (_isBuildCalled) {
            return;
        }
        _isBuildCalled = true;

        auto exe = _dep->output();
        if (exe.empty() || _dep->target()->name() == "root") {
            return;
        }

        _dep->dirty(false);

        time_t lastDependency = 0;
        for (auto &d : _dep->dependencies()) {
            auto t = d->changedTime(files);
            if (d->dirty()) {
                _dep->dirty(true);
            }
            lastDependency = std::max(t, lastDependency);
            if (t == 0) {
                _dep->dirty(true);
            }
        }

        if (lastDependency > _dep->changedTime(files)) {
            _dep->dirty(true);
        }
        else if (!_dep->dirty()) {
            std::cout << _dep->output() << " is fresh " << std::endl;
        }

        prepareCommand();

        std::vector<std::string> oldDependencies;
        std::string oldCommand;
        tie(oldDependencies, oldCommand) = files.parseDepFile(_dep->depFile());
        if (_dep->command() != oldCommand) {
            dout << _dep->output() << " command differs \n";
            dout << _dep->command() << "\n";
            dout << oldCommand << "\n\n";
            _dep->dirty(true);
        }
    }

    std::string work(const IFiles &files, IThreadPool &pool) override {
        if (!_dep->command().empty()) {
            {
                files.replaceFile(_dep->depFile(), _dependencyString);
            }
            return _dep->work(files, pool);
        }

        return {};
    }

    IDependency &dependency() override {
        return *_dep;
    }

private:
    void prepareCommand() {
        Token fileList;

        for (auto f : _dep->dependencies()) {
            if (f->includeInBinary()) {
                fileList += (f->linkString() + " ");
            }
        }

        _dependencyString = prepareDependencyString();

        auto cpp = _dep->target()->getCompiler("cpp");

        auto exe = _dep->output();
        auto buildType = _dep->target()->buildType();
        Token cmd;
        if (buildType == Shared) {
            cmd = cpp + " -shared -o " + exe + " -Wl,--start-group " +
                  fileList + " " + _dep->target()->getLibs() +
                  "  -Wl,--end-group  " + _dep->target()->getFlags();
        }
        else if (buildType == Static) {
            cmd = "ar -rs " + exe + " " + fileList;
            if (globals.verbose) {
                cmd += " -v ";
            }
        }
        else {
            cmd = cpp + " -o " + exe + " -Wl,--start-group " + fileList + " " +
                  _dep->target()->getLibs() + "  -Wl,--end-group  " +
                  _dep->target()->getFlags();
        }
        cmd = _dep->target()->preprocessCommand(cmd);

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

        _dep->command(cmd);
    }

    std::string prepareDependencyString() const {
        std::ostringstream ss;
        ss << _dep->output() << ":";
        for (auto &d : _dep->dependencies()) {
            ss << " " << d->output();
        }
        ss << "\n";
        return ss.str();
    }

    //! This is to check if should include linker -rpath or similar
    bool hasReferencesToSharedLibrary() const {
        for (auto &d : _dep->dependencies()) {
            if (d->buildType() == Shared) {
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<IDependency> _dep;
    bool _isBuildCalled = false;
    ICompiler *_compilerType;
    std::string _dependencyString;
};

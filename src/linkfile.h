/*
 * linkfile.h
 *
 *  Created on: 25 jan. 2020
 *      Author: Mattias Larsson Sköld
 */

#pragma once

#include "compilertype.h"
#include "dependency.h"

class LinkFile : public Dependency {
    bool _isBuildCalled = false;
    ICompiler *_compilerType;

public:
    LinkFile(const LinkFile &) = delete;
    LinkFile(LinkFile &&) = delete;
    LinkFile(Token filename, IBuildTarget *target, ICompiler *compilerType)
        : Dependency(target), _compilerType(compilerType) {
        output(removeDoubleDots(target->getOutputDir() + filename));
    }

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
            cout << output() << " is fresh " << endl;
        }

        if (dirty()) {
            Token fileList;

            for (auto f : dependencies()) {
                if (f->includeInBinary()) {
                    fileList += (f->linkString() + " ");
                }
            }

            auto cpp = target()->getCompiler("cpp");

            auto out = target()->get("out");
            auto buildType = target()->buildType();
            Token cmd;
            if (buildType == Shared) {
                cmd = cpp + " -shared -o " + exe + " -Wl,--start-group " +
                      fileList + " " + target()->getLibs() +
                      "  -Wl,--end-group  " + target()->getFlags();
            }
            else if (buildType == Static) {
                cmd = "ar -rs " + exe + " " + fileList;
                if (globals.verbose) {
                    cmd += " -v ";
                }
            }
            else {
                cmd = cpp + " -o " + exe + " -Wl,--start-group " + fileList +
                      " " + target()->getLibs() + "  -Wl,--end-group  " +
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
            command(cmd);
        }
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
};

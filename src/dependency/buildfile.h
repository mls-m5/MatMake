// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "dependency.h"
#include "dependency/ibuildrule.h"
#include "environment/globals.h"
#include "environment/popenstream.h"
#include "environment/prescan.h"
#include "main/mdebug.h"
#include "target/ibuildtarget.h"

//! Represent a single source file to be built
class BuildFile : public IBuildRule {
public:
    enum Type {
        CppToO,
        CppToPcm,
        PcmToO,
    };

    BuildFile(const BuildFile &) = delete;
    BuildFile(BuildFile &&) = delete;
    BuildFile(Token filename,
              IBuildTarget *target,
              Type type,
              std::unique_ptr<IDependency> dependency = nullptr)
        : _dep(dependency ? std::move(dependency)
                          : std::make_unique<Dependency>(
                                target, type != CppToPcm, Object))
        , _filetype(stripFileEnding(filename).second)
        , _type(type) {
        auto withoutEnding =
            stripFileEnding(target->getBuildDirectory() + filename);
        if (withoutEnding.first.empty()) {
            throw MatmakeError(filename,
                               "could not figure out source file type '" +
                                   target->getBuildDirectory() + filename +
                                   "' . Is the file ending right?");
        }

        if (type == CppToPcm) {
            _dep->output(fixPcmEnding(filename));
        }
        else {
            _dep->output(fixObjectEnding(filename));
        }

        _dep->depFile(Dependency::fixDepEnding(_dep->output()));

        if (type == PcmToO) {
            _dep->input(fixPcmEnding(filename));
        }
        else {
            _dep->input(filename);
        }

        if (filename.empty()) {
            throw MatmakeError(filename, "empty buildfile added");
        }

        if (_dep->output().empty()) {
            throw MatmakeError(filename, "could not find target name");
        }

        if (_dep->depFile().empty()) {
            throw MatmakeError(
                filename, "could not find dep filename for " + _dep->output());
        }
    }

    void prescan(
        IFiles &files,
        const std::vector<std::unique_ptr<IBuildRule>> &buildFiles) override {

        if (files.getTimeChanged(_dep->depFile()) >
            _dep->inputChangedTime(files)) {
            return;
        }

        if (_type == CppToPcm) {
            auto command = _dep->target()->getCompiler(_filetype) + " " +
                           _dep->input() + " -E " + getFlags() + " 2>/dev/null";

            POpenStream stream(command);

            dout << "prescan command: " << command << std::endl;

            auto deps = ::prescan(stream);

            dout << "this should be printed to .d-file\n";
            for (auto &include : deps.includes) {
                dout << include << " ";
            }

            dout << "\n imports:\n";

            std::ostringstream depFileStream;

            depFileStream << _dep->output() << ":";

            for (auto &imp : deps.imports) {
                dout << imp << " ";

                depFileStream << " " << imp << ".pcm";

                auto importFilename = imp + ".pcm"; // Fix mapping

                for (auto &bf : buildFiles) {
                    if (bf->dependency().output() == importFilename) {
                        _dep->addDependency(&bf->dependency());
                        bf->dependency().addSubscriber(_dep.get());
                        break;
                    }
                }
            }

            depFileStream << " " << _dep->input();

            for (auto &include : deps.includes) {
                depFileStream << " " << include;
            }

            depFileStream << "\n";

            if (files.getTimeChanged(_dep->depFile()) <
                _dep->inputChangedTime(files)) {
                files.replaceFile(_dep->depFile(), depFileStream.str());
            }

            dout << std::endl;

            _dep->shouldAddCommandToDepFile(true);
        }
        else if (_type == PcmToO) {
            files.replaceFile(_dep->depFile(),
                              _dep->output() + ": " + _dep->input() + "\n");

            for (auto &bf : buildFiles) {
                if (bf->dependency().output() == _dep->input()) {
                    dout << "adding own pcm dependency "
                         << bf->dependency().output() << " to "
                         << _dep->output() << "\n";
                    _dep->addDependency(&bf->dependency());
                    bf->dependency().addSubscriber(_dep.get());
                    break;
                }
            }

            _dep->shouldAddCommandToDepFile(true);
        }
    }

    Token createCommand() {
        Token depCommand;

        //! If not using prescan
        if (_type == CppToO) {
            depCommand = " -MMD -MF " + _dep->depFile() + " ";
            depCommand.location = _dep->input().location;
        }

        Token command = _dep->target()->getCompiler(_filetype) +
                        ((_type == CppToPcm) ? " --precompile " : " -c ") +
                        " -o " + _dep->output() + " " + _dep->input() + " " +
                        getFlags() + depCommand;

        if (_dep->target()->hasModules()) {
            if (_type == CppToPcm || _type == CppToO) {
                command += " -fprebuilt-module-path=. ";
            }
        }

        command.location = _dep->input().location;

        return preprocessCommand(command);
    }

    void prepare(const IFiles &files) override {
        time_t outputChangedTime = _dep->changedTime(files);
        if (outputChangedTime < _dep->inputChangedTime(files)) {
            _dep->dirty(true);
            dout << "file is dirty" << std::endl;
        }

        std::vector<std::string> dependencyFiles;
        std::string oldCommand;
        tie(dependencyFiles, oldCommand) = files.parseDepFile(_dep->depFile());

        if (dependencyFiles.empty()) {
            dout << "file is dirty" << std::endl;
            _dep->dirty(true);
        }
        else {
            for (auto &d : dependencyFiles) {
                auto dependencyTimeChanged = files.getTimeChanged(d);
                if (dependencyTimeChanged == 0 ||
                    dependencyTimeChanged > outputChangedTime) {
                    dout << "rebuilding because older than " << d << std::endl;
                    _dep->dirty(true);
                    break;
                }
            }
        }

        auto command = createCommand();

        _dep->command(command);

        if (!_dep->dirty() && command != oldCommand) {
            dout << "command is changed for " << _dep->output() << std::endl;
            dout << " old: " << oldCommand << "\n";
            dout << " new: " << command << "\n";
            _dep->dirty(true);
        }
    }

    IDependency &dependency() override {
        return *_dep;
    }

    std::string work(const IFiles &files, class ThreadPool &pool) override {
        return _dep->work(files, pool, *this);
    }

private:
    std::unique_ptr<IDependency> _dep;
    Token _filetype; // The ending of the filename
    Type _type = CppToO;

    Token fixObjectEnding(Token filename) {
        return removeDoubleDots(_dep->target()->getBuildDirectory() + filename +
                                ".o");
    }

    Token fixPcmEnding(Token filename) {
        return removeDoubleDots(_dep->target()->getBuildDirectory() +
                                stripFileEnding(filename).first + ".pcm");
    }

    Token preprocessCommand(Token command) const {
        return Token(trim(_dep->target()->preprocessCommand(command)),
                     command.location);
    }

    Token getFlags() {
        return _dep->target()->getBuildFlags(_filetype);
    }

    //    BuildType buildType() const override {
    //        return Object;
    //    }
};

// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "dependency.h"
#include "environment/globals.h"
#include "environment/popenstream.h"
#include "environment/prescan.h"
#include "main/mdebug.h"
#include "target/ibuildtarget.h"

//! Represent a single source file to be built
class BuildFile : public Dependency {
public:
    enum Type {
        CppToO,
        CppToPcm,
        PcmToO,
    };

    BuildFile(const BuildFile &) = delete;
    BuildFile(BuildFile &&) = delete;
    BuildFile(Token filename, IBuildTarget *target, Type type)
        : Dependency(target)
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
            output(fixPcmEnding(filename));
        }
        else {
            output(fixObjectEnding(filename));
        }

        depFile(fixDepEnding(output()));

        if (type == PcmToO) {
            input(fixPcmEnding(filename));
        }
        else {
            input(filename);
        }

        if (filename.empty()) {
            throw MatmakeError(filename, "empty buildfile added");
        }

        if (output().empty()) {
            throw MatmakeError(filename, "could not find target name");
        }

        if (depFile().empty()) {
            throw MatmakeError(filename,
                               "could not find dep filename for " + output());
        }
    }

    void prescan(
        IFiles &files,
        const std::vector<std::unique_ptr<IDependency>> &buildFiles) override {

        if (files.getTimeChanged(depFile()) > inputChangedTime(files)) {
            return;
        }

        if (_type == CppToPcm) {
            auto command = target()->getCompiler(_filetype) + " " + input() +
                           " -E " + getFlags() + " 2>/dev/null";

            POpenStream stream(command);

            dout << "prescan command: " << command << std::endl;

            auto deps = ::prescan(stream);

            dout << "this should be printed to .d-file\n";
            for (auto &include : deps.includes) {
                dout << include << " ";
            }

            dout << "\n imports:\n";

            std::ostringstream depFileStream;

            depFileStream << output() << ":";

            for (auto &imp : deps.imports) {
                dout << imp << " ";

                depFileStream << " " << imp << ".pcm";

                auto importFilename = imp + ".pcm"; // Fix mapping

                for (auto &bf : buildFiles) {
                    if (bf->output() == importFilename) {
                        addDependency(bf.get());
                        bf->addSubscriber(this);
                        break;
                    }
                }
            }

            depFileStream << " " << input();

            for (auto &include : deps.includes) {
                depFileStream << " " << include;
            }

            depFileStream << "\n";

            if (files.getTimeChanged(depFile()) < inputChangedTime(files)) {
                files.replaceFile(depFile(), depFileStream.str());
            }

            dout << std::endl;

            shouldAddCommandToDepFile(true);
        }
        else if (_type == PcmToO) {
            files.replaceFile(depFile(), output() + ": " + input() + "\n");

            for (auto &bf : buildFiles) {
                if (bf->output() == input()) {
                    dout << "adding own pcm dependency " << bf->output()
                         << " to " << output() << "\n";
                    addDependency(bf.get());
                    bf->addSubscriber(this);
                    break;
                }
            }

            shouldAddCommandToDepFile(true);
        }
    }

    Token createCommand() {
        Token depCommand;

        //! If not using prescan
        if (_type == CppToO) {
            depCommand = " -MMD -MF " + depFile() + " ";
            depCommand.location = input().location;
        }

        Token command = target()->getCompiler(_filetype) +
                        ((_type == CppToPcm) ? " --precompile " : " -c ") +
                        " -o " + output() + " " + input() + " " + getFlags() +
                        depCommand;

        if (target()->hasModules()) {
            if (_type == CppToPcm || _type == CppToO) {
                command += " -fprebuilt-module-path=. ";
            }
        }

        command.location = input().location;

        return preprocessCommand(command);
    }

    void prepare(const IFiles &files) override {
        time_t outputChangedTime = changedTime(files);
        if (outputChangedTime < inputChangedTime(files)) {
            dirty(true);
            dout << "file is dirty" << std::endl;
        }

        std::vector<std::string> dependencyFiles;
        std::string oldCommand;
        tie(dependencyFiles, oldCommand) = parseDepFile(files);

        if (dependencyFiles.empty()) {
            dout << "file is dirty" << std::endl;
            dirty(true);
        }
        else {
            for (auto &d : dependencyFiles) {
                auto dependencyTimeChanged = files.getTimeChanged(d);
                if (dependencyTimeChanged == 0 ||
                    dependencyTimeChanged > outputChangedTime) {
                    dout << "rebuilding because older than " << d << std::endl;
                    dirty(true);
                    break;
                }
            }
        }

        auto command = createCommand();

        this->command(command);

        if (!dirty() && command != oldCommand) {
            dout << "command is changed for " << output() << std::endl;
            dout << " old: " << oldCommand << "\n";
            dout << " new: " << command << "\n";
            dirty(true);
        }
    }

    bool includeInBinary() const override {
        return _type != CppToPcm;
    }

private:
    Token _filetype; // The ending of the filename
    Type _type = CppToO;

    Token fixObjectEnding(Token filename) {
        return removeDoubleDots(target()->getBuildDirectory() + filename +
                                ".o");
    }

    Token fixPcmEnding(Token filename) {
        return removeDoubleDots(target()->getBuildDirectory() +
                                stripFileEnding(filename).first + ".pcm");
    }

    Token preprocessCommand(Token command) const {
        return Token(trim(target()->preprocessCommand(command)),
                     command.location);
    }

    Token getFlags() {
        return target()->getBuildFlags(_filetype);
    }

    BuildType buildType() const override {
        return Object;
    }
};

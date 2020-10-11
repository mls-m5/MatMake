// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "dependency.h"
#include "environment/globals.h"
#include "environment/popenstream.h"
#include "environment/prescan.h"
#include "main/mdebug.h"
#include "target/ibuildtarget.h"

#include <fstream>

//! Represent a single source file to be built
class BuildFile : public Dependency {
    Token _filetype; // The ending of the filename
public:
    enum Type {
        RegularCpp,
        CppToPcm,
        PcmToO,
    };

    BuildFile(const BuildFile &) = delete;
    BuildFile(BuildFile &&) = delete;
    BuildFile(Token filename, IBuildTarget *target, Type type)
        : Dependency(target)
        , _filetype(stripFileEnding(filename).second) {
        auto withoutEnding =
            stripFileEnding(target->getBuildDirectory() + filename);
        if (withoutEnding.first.empty()) {
            throw MatmakeError(filename,
                               "could not figure out source file type '" +
                                   target->getBuildDirectory() + filename +
                                   "' . Is the file ending right?");
        }

        {
            if (type == CppToPcm) {
                output(fixPcmEnding(filename));
            }
            else {
                output(fixObjectEnding(filename));
            }
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

    Token fixObjectEnding(Token filename) {
        return removeDoubleDots(target()->getBuildDirectory() + filename +
                                ".o");
    }

    Token fixPcmEnding(Token filename) {
        return removeDoubleDots(target()->getBuildDirectory() + filename +
                                ".pcm");
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

    void prescan(IFiles &files) override {
        //        auto inputChangedTime = Dependency::inputChangedTime(files);
        //        if (inputChangedTime < files.getTimeChanged(depFile())) {
        //            return;
        //        }

        //        auto command = target()->getCompiler(_filetype) + " " +
        //        input() +
        //                       " " + getFlags();

        //        POpenStream stream(command);

        //        dout << "prescan command: " << command << std::endl;

        //        auto deps = ::prescan(stream);
    }

    void prepare(const IFiles &files) override {
        time_t outputChangedTime = changedTime(files);
        if (outputChangedTime < inputChangedTime(files)) {
            dirty(true);
            dout << "file is dirty" << std::endl;
        }

        Token depCommand;

        if (target()->hasModules()) {
            // Generate .d-file
        }
        else {
            depCommand = " -MMD -MF " + depFile() + " ";
            depCommand.location = input().location;
        }

        std::vector<std::string> dependencyFiles;
        std::string oldCommand;
        tie(dependencyFiles, oldCommand) = parseDepFile();

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

        auto flags = getFlags();
        Token command = target()->getCompiler(_filetype) + " -c -o " +
                        output() + " " + input() + " " + flags + depCommand;

        command = preprocessCommand(command);
        command.location = input().location;
        this->command(command);

        if (!dirty() && command != oldCommand) {
            dout << "command is changed for " << output() << std::endl;
            dirty(true);
        }
    }
};

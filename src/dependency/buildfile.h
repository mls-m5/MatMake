// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "dependency.h"
#include "environment/globals.h"
#include "main/mdebug.h"
#include "target/ibuildtarget.h"

#include <fstream>

//! Represent a single source file to be built
class BuildFile : public Dependency {
    Token _filename; // The source of the file
    Token _filetype; // The ending of the filename
public:
    BuildFile(const BuildFile &) = delete;
    BuildFile(BuildFile &&) = delete;
    BuildFile(Token filename, IBuildTarget *target)
        : Dependency(target), _filename(filename),
          _filetype(stripFileEnding(filename).second) {
        auto withoutEnding =
            stripFileEnding(target->getBuildDirectory() + filename);
        if (withoutEnding.first.empty()) {
            throw MatmakeError(filename,
                               "could not figure out source file type '" +
                                   target->getBuildDirectory() + filename +
                                   "' . Is the file ending right?");
        }

        Dependency::output(removeDoubleDots(
            fixObjectEnding(target->getBuildDirectory() + filename)));
        depFile(fixDepEnding(output()));

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

    static Token fixObjectEnding(Token filename) {
        return stripFileEnding(filename).first + ".o";
    }

    time_t getInputChangedTime(const IFiles &files) {
        return files.getTimeChanged(_filename);
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

    void prepare(const IFiles &files) override {
        using namespace std;
        auto inputChangedTime = getInputChangedTime(files);
        vector<string> dependencyFiles;
        string oldCommand;
        tie(dependencyFiles, oldCommand) = parseDepFile();
        time_t outputChangedTime = changedTime(files);

        if (outputChangedTime < inputChangedTime) {
            dirty(true);
            dout << "file is dirty" << endl;
        }

        Token depCommand = " -MMD -MF " + depFile() + " ";
        depCommand.location = _filename.location;

        if (dependencyFiles.empty()) {
            dout << "file is dirty" << endl;
            dirty(true);
        }
        else {
            for (auto &d : dependencyFiles) {
                auto dependencyTimeChanged = files.getTimeChanged(d);
                if (dependencyTimeChanged == 0 ||
                    dependencyTimeChanged > outputChangedTime) {
                    dout << "rebuilding because older than " << d << endl;
                    dirty(true);
                    break;
                }
            }
        }

        auto flags = getFlags();
        Token command = target()->getCompiler(_filetype) + " -c -o " +
                        output() + " " + _filename + " " + flags + depCommand;

        command = preprocessCommand(command);
        command.location = _filename.location;
        this->command(command);

        if (!dirty() && command != oldCommand) {
            dout << "command is changed for " << output() << endl;
            dirty(true);
        }
    }
};
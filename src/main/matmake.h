// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "createproject.h"
#include "dependency/dependency.h"
#include "environment/environment.h"
#include "environment/files.h"
#include "environment/globals.h" // Global variables
#include "environment/locals.h"
#include "help.h"
#include "main/token.h"
#include "parsearguments.h"
#include "parsematmakefile.h"
#include "target/buildtarget.h"

//! Do what operation was given in command line arguments and saved in locals
ShouldQuitT work(const Locals &locals, IEnvironment &environment) {
    try {
        if (locals.operation == "build") {
            environment.compile(locals.targets);
        }
        else if (locals.operation == "list") {
            environment.listAlternatives();
            return true;
        }
        else if (locals.operation == "clean") {
            environment.clean(locals.targets);
        }
        else if (locals.operation == "rebuild") {
            environment.clean(locals.targets);
            if (!globals.bailout) {
                environment.compile(locals.targets);
            }
        }
    }
    catch (MatmakeError &e) {
        std::cerr << e.what() << "\n";
    }
    catch (std::runtime_error &e) {
        std::cerr << e.what() << "\n";
    }

    return false;
}

//! The main entry point for a project.
//!
//! Runs once in the working directory and once in each directory specified
//! with the import keyword
int start(std::vector<std::string> args) {
    auto startTime = time(nullptr);

    Locals locals;
    {
        ShouldQuitT shouldQuit;
        IsErrorT isError;
        std::tie(shouldQuit, isError) = parseArguments(args, locals);

        if (isError) {
            return -1;
        }

        if (shouldQuit) {
            return 0;
        }
    }

    auto files = std::make_shared<Files>();
    Environment environment(files);

    {
        ShouldQuitT shouldQuit;
        IsErrorT isError;
        TargetPropertyCollection properties{};
        std::tie(shouldQuit, isError, properties) =
            parseMatmakeFile(locals, *files);

        if (shouldQuit) {
            return 0;
        }
        if (isError) {
            globals.bailout = true;
        }

        environment.setTargetProperties(move(properties));
    }

    if (!globals.bailout) {
        if (work(locals, environment)) {
            return 0;
        }
    }

    auto endTime = time(nullptr);

    auto duration = endTime - startTime;
    auto m = duration / 60;
    auto s = duration - m * 60;

    std::cout << "\n";
    if (globals.bailout) {
        std::cout << "failed... " << m << "m " << s << "s\n";
        return -1;
    }
    else {
        std::string doneMessage = "done...";
        if (locals.operation == "clean") {
            doneMessage = "cleaned...";
        }
        std::cout << doneMessage << " " << m << "m " << s << "s\n";
        return 0;
    }
}

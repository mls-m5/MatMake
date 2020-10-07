#pragma once

#include "environment/globals.h"
#include "environment/locals.h"
#include "main/createproject.h"
#include "main/help.h"
#include <iostream>

IsErrorT tokenizeMatmakeFile() {
    std::ifstream matmakefile("Matmakefile");

    if (!matmakefile.is_open()) {
        return true;
    }

    for (int lineNumber = 1; matmakefile; ++lineNumber) {
        std::string line;
        getline(matmakefile, line);
        auto words = tokenize(line, lineNumber);

        for (auto word : words) {
            std::cout << word.location.line << ":" << word.location.col << " "
                      << word << " '" << word.trailingSpace << "'"
                      << "\n";
        }
    }

    return false;
}

//! Parse arguments and produce a new Locals object containing the result
//!
//! Also alters globals object if any options related to that is used
std::tuple<ShouldQuitT, IsErrorT> parseArguments(std::vector<std::string> args,
                                                 Locals &locals) {
    using namespace std;

    ShouldQuitT shouldQuit = false;
    IsErrorT isError = false;

    locals.args = args;

    for (size_t i = 0; i < args.size(); ++i) {
        string arg = args[i];
        if (arg == "clean") {
            locals.operation = "clean";
        }
        else if (arg == "rebuild") {
            locals.operation = "rebuild";
        }
        else if (arg == "--local") {
            locals.localOnly = true;
        }
        else if (arg == "-v" || arg == "--verbose") {
            globals.verbose = true;
        }
        else if (arg == "--list" || arg == "-l") {
            locals.operation = "list";
        }
        else if (arg == "--help" || arg == "-h") {
            cout << helpText << endl;
            shouldQuit = true;
            break;
        }
        else if (arg == "--example") {
            cout << "### Example matmake file (Matmakefile): " << endl;
            cout << "'#' means comment" << endl;
            cout << exampleMatmakefile << endl;
            cout << "### Example main file (src/main.cpp):" << endl;
            cout << exampleMain << endl;
            shouldQuit = true;
            break;
        }
        else if (arg == "--debug" || arg == "-d") {
            globals.debugOutput = true;
            globals.verbose = true;
        }
        else if (arg == "-j") {
            ++i;
            if (i < args.size()) {
                globals.numberOfThreads =
                    static_cast<unsigned>(atoi(args[i].c_str()));
            }
            else {
                cerr << "expected number of threads after -j argument" << endl;
                isError = true;
                break;
            }
        }
        else if (arg.size() > 2 && arg.substr(0, 2) == "-j" &&
                 (arg[2] >= '0' && arg[3] < '9')) {
            globals.numberOfThreads = std::stoi(arg.substr(2));
        }
        else if (arg == "--init") {
            ++i;
            if (i < args.size()) {
                string arg = args[i];
                if (arg[0] != '-') {
                    isError = createProject(args[i]);
                    break;
                }
            }
            isError = createProject("");
            break;
        }
        else if (arg == "all") {
            // Do nothing. Default is to build all
        }
        else if (arg == "--tokenize") {
            // For debugging
            tokenizeMatmakeFile();
            shouldQuit = true;
            break;
        }
        else if (arg.find("=") != string::npos) {
            auto f = arg.find("=");
            if (f == 0) {
                continue;
            }
            if (f == arg.size() - 1) {
                continue;
            }

            auto varName = arg.substr(0, f);
            auto value = arg.substr(f + 1);

            locals.vars[varName].push_back(value);
        }
        else {
            locals.targets.push_back(arg);
        }
    }

    return std::tuple<ShouldQuitT, IsErrorT>{shouldQuit, isError};
}

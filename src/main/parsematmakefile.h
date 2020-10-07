#pragma once

#include "environment/ienvironment.h"
#include "environment/ifiles.h"
#include "environment/locals.h"
#include "main/mdebug.h"
#include "matmake-common.h"
#include <fstream>
#include <iostream>

bool isOperator(std::string &op) {
    std::vector<std::string> opList = {
        "=",
        "+=",
        "-=",
    };
    return find(opList.begin(), opList.end(), op) != opList.end();
}

//! Parse the Matmakefile (obviously). Put result in environment.
std::tuple<ShouldQuitT, IsErrorT> parseMatmakeFile(const Locals &locals,
                                                   IEnvironment &environment,
                                                   const IFiles &files) {
    ShouldQuitT shouldQuit = false;
    IsErrorT isError = false;

    std::ifstream matmakefile("Matmakefile");
    if (!matmakefile.is_open()) {
        if (files.getTimeChanged("Makefile") ||
            files.getTimeChanged("makefile")) {
            std::cout << "makefile in " << files.getCurrentWorkingDirectory()
                      << "\n";
            std::string arguments = "make";
            for (auto arg : locals.args) {
                arguments += (" " + arg);
            }
            arguments += " -j";
            system(arguments.c_str());
            std::cout << "\n";
        }
        else {
            std::cout << "matmake: could not find Matmakefile in "
                      << files.getCurrentWorkingDirectory() << "\n";
        }
        shouldQuit = true;
        return std::tuple<ShouldQuitT, IsErrorT>{shouldQuit, isError};
    }

    environment.setCommandLineVars(locals.vars);

    int lineNumber = 1;

    auto getMultilineArgument = [&lineNumber, &matmakefile]() {
        Tokens ret;
        std::string line;
        // Continue as long as the line starts with a space character
        while (matmakefile && isspace(matmakefile.peek())) {
            ++lineNumber;
            getline(matmakefile, line);
            auto words = tokenize(line, lineNumber);
            ret.append(words);
            if (!ret.empty()) {
                ret.back().trailingSpace += " ";
            }
        }

        return ret;
    };

    for (std::string line; getline(matmakefile, line); ++lineNumber) {
        auto words = tokenize(line, lineNumber);

        if (!words.empty()) {
            auto it = words.begin() + 1;
            for (; it != words.end(); ++it) {
                if (isOperator(*it)) {
                    break;
                }
            }
            if (it != words.end()) {
                auto argumentStart = it + 1;
                std::vector<Token> variableName(words.begin(), it);
                Tokens value(argumentStart, words.end());
                auto variableNameString = concatTokens(words.begin(), it);

                if (value.empty()) {
                    value = getMultilineArgument();
                }

                if (*it == "=") {
                    environment.setVariable(variableName, value);
                }
                else if (*it == "+=") {
                    environment.appendVariable(variableName, value);
                }
            }
            else if (!locals.localOnly && words.size() >= 2 &&
                     words.front() == "external") {
                vout << "external dependency to " << words[1] << "\n";

                environment.addExternalDependency(
                    false, words[1], Tokens(words.begin() + 2, words.end()));
            }
            else if (!locals.localOnly && words.size() >= 2 &&
                     words.front() == "dependency") {
                environment.addExternalDependency(
                    true, words[1], Tokens(words.begin() + 2, words.end()));
            }
            else if (words.empty()) {
            }
            else {
                std::cerr << words.front().getLocationDescription() << ":";
                std::cerr << "'" << line << "': are you missing operator?"
                          << "\n";
            }
        }
    }

    return std::tuple<ShouldQuitT, IsErrorT>{shouldQuit, isError};
}

#pragma once

#include "environment/ienvironment.h"
#include "environment/ifiles.h"
#include "environment/locals.h"
#include "main/mdebug.h"
#include "matmake-common.h"
#include "target/targetproperties.h"
#include <fstream>
#include <iostream>

inline bool isOperator(const std::string &op) {
    std::vector<std::string> opList = {
        "=",
        "+=",
        "-=",
    };
    return find(opList.begin(), opList.end(), op) != opList.end();
}

inline bool isColon(const std::string &str) {
    return str == ":";
}

//! Parse the Matmakefile (obviously). Put result in environment.
std::tuple<ShouldQuitT, IsErrorT, TargetPropertyCollection> parseMatmakeFile(
    const Locals &locals, const IFiles &files) {
    ShouldQuitT shouldQuit = false;
    IsErrorT isError = false;

    auto matmakefile = files.openRead("Matmakefile");
    if (!matmakefile) {
        if (files.getTimeChanged("Makefile") ||
            files.getTimeChanged("makefile")) {
            std::cout << "makefile in " << files.currentDirectory() << "\n";
            std::string arguments = "make";
            for (auto arg : locals.args) {
                arguments += (" " + arg);
            }
            arguments += " -j";
            files.system(arguments.c_str());
            std::cout << "\n";
        }
        else {
            std::cout << "matmake: could not find Matmakefile in "
                      << files.currentDirectory() << "\n";
        }
        shouldQuit = true;
        return {shouldQuit, isError, TargetPropertyCollection{locals.vars}};
    }

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

    TargetPropertyCollection properties(locals.vars);

    for (std::string line; getline(matmakefile, line); ++lineNumber) {
        auto words = tokenize(line, lineNumber);

        if (!words.empty()) {
            auto it = words.begin() + 1;
            auto begin = words.begin();

            std::string config;
            bool invertConfigLogic = false;

            // Check if there is any ':'
            for (auto jt = it; jt != words.end(); ++jt) {
                if (isColon(*it)) {
                    config = words.front();
                    if (!config.empty() && config.front() == '!') {
                        invertConfigLogic = true;
                        config = config.substr(1); // Strip of '!'
                    }
                    ++jt;
                    begin = jt;
                    it = begin;
                    ++it;
                    break;
                }
            }

            // Find '=' or similar
            for (; it != words.end(); ++it) {
                if (isOperator(*it)) {
                    break;
                }
            }
            if (it != words.end()) {
                auto argumentStart = it + 1;
                Tokens variableName(begin, it);
                Tokens value(argumentStart, words.end());
                auto variableNameString = variableName.concat();

                if (value.empty()) {
                    value = getMultilineArgument();
                }

                if (invertConfigLogic) {
                    if (!config.empty() && config == locals.config) {
                        continue;
                    }
                }
                else if (!config.empty() && config != locals.config) {
                    continue;
                }

                if (*it == "=") {
                    properties.setVariable(variableName, value);
                }
                else if (*it == "+=") {
                    properties.appendVariable(variableName, value);
                }
            }
            else if (!locals.localOnly && words.size() >= 2 &&
                     words.front() == "external") {
                vout << "external dependency to " << words[1] << "\n";

                // Todo: Fix this again
                //                environment.addExternalDependency(
                //                    false, words[1], Tokens(words.begin() + 2,
                //                    words.end()));
            }
            else if (!locals.localOnly && words.size() >= 2 &&
                     words.front() == "dependency") {
                // Todo: fix this again
                //                environment.addExternalDependency(
                //                    true, words[1], Tokens(words.begin() + 2,
                //                    words.end()));
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

    return {shouldQuit, isError, move(properties)};
}

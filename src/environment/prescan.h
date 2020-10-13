//! Copyright Mattias Larsson Sköld 2020

#pragma once

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

struct PrescanResult {
    std::vector<std::string> includes;
    std::vector<std::string> systemHeaders;
    std::vector<std::string> imports;
    std::vector<std::string> exportModules;
};

PrescanResult prescan(std::istream &input) {
    PrescanResult res;

    auto addUnique = [](std::vector<std::string> &collection, auto &&value) {
        if (std::find(collection.begin(), collection.end(), value) ==
            collection.end()) {
            collection.emplace_back(move(value));
        }
    };

    for (std::string line; getline(input, line);) {
        if (line.size() < 3) {
            continue;
        }

        if (line.front() == '#' && line[1] == ' ') {
            auto begin = line.find("\"");
            auto end = line.rfind("\"");

            if (begin == std::string::npos) {
                continue;
            }

            auto name = line.substr(begin + 1, end - begin - 1);

            if (name.empty()) {
                continue;
            }
            if (name.front() == '<') {
                continue;
            }
            else if (name.front() == '/') {
                // todo: Do some better check for this in future
                //                res.systemHeaders.emplace_back(move(name));

                addUnique(res.systemHeaders, move(name));
            }
            else {
                //                res.includes.emplace_back(move(name));
                addUnique(res.includes, move(name));
            }
        }
        else if (line.rfind("import ", 0) != std::string::npos) {
            auto moduleName = line.substr(7);
            auto f = moduleName.rfind(";");
            if (f != std::string::npos) {
                moduleName.erase(f, moduleName.size());
                //                res.imports.push_back(move(moduleName));
                addUnique(res.imports, move(moduleName));
            }
        }
        else if (line.rfind("export module ", 0) != std::string::npos) {
            auto moduleName = line.substr(14);
            auto f = moduleName.rfind(";");
            if (f != std::string::npos) {
                moduleName.erase(f, moduleName.size());
                //                res.exportModules.push_back(move(moduleName));
                addUnique(res.exportModules, move(moduleName));
            }
        }
    }

    return res;
}

#pragma once

#include "environment/files.h"
#include "main/help.h"
#include <fstream>
#include <sstream>
#include <string>

std::string stripComments(const std::string code) {
    std::istringstream ss(code);
    std::ostringstream out;

    auto parseLine = [](std::string line) -> std::string {
        if (line.empty()) {
            return "";
        }
        else if (line.front() == '#') {
            return "x";
        }
        else {
            return line;
        }
    };

    std::string line;

    for (size_t i = 0; i < 3; ++i) {
        // Keep the comments on the first three lines
        std::getline(ss, line);
        out << line << "\n";
    }

    while (std::getline(ss, line)) {
        line = parseLine(line);
        if (line != "x") {
            out << line << "\n";
        }
    }

    return out.str();
}

//! Creates a project in the current folder
//!
//! @returns 0 on success anything else on error
int createProject(std::string dir) {
    Files files;
    if (!dir.empty()) {
        files.createDirectory(dir);

        std::error_code errorCode;
        filesystem::create_directories(dir.c_str(), errorCode);
        if (errorCode){
            std::cerr << "could not create directory " << dir << "\n";
            return -1;
        }
    }

    if (files.getTimeChanged("Matmakefile") > 0) {
        std::cerr << "Matmakefile already exists. Exiting..."
                  << "\n";
        return -1;
    }

    {
        std::ofstream file("Matmakefile");
        file << stripComments(exampleMatmakefile);
    }

    if (files.getTimeChanged("Makefile") > 0) {
        std::cerr << "Makefile already exists. Exiting...\n";
        return -1;
    }

    {
        std::ofstream file("Makefile");
        file << exampleMakefile;
    }

    files.createDirectory("src");

    if (files.getTimeChanged("src/main.cpp") > 0) {
        std::cerr << "src/main.cpp file already exists. Exiting...\n";
        return -1;
    }

    files.createDirectory("include");

    {
        std::ofstream file("src/main.cpp");
        file << exampleMain;
    }

    std::cout << "project created";
    if (!dir.empty()) {
        std::cout << " in " << dir;
    }
    std::cout << "...\n";
    return 0;
}

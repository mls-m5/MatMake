#pragma once

#include <map>
#include <string>
#include <vector>

//! Variables that is to be used by each start function
struct Locals {
    std::vector<std::string> targets; // What to build
    std::vector<std::string> args;    // Copy of command line arguments
    std::map<std::string, std::vector<std::string>>
        vars; // Variables with values from commandline
    std::vector<std::string>
        externalDependencies; // Dependencies to build before main build
    std::vector<std::string>
        external; // External dependencies to build after main build
    std::string operation = "build";
    bool localOnly = false;
};

typedef bool IsErrorT;
typedef bool ShouldQuitT;

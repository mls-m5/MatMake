// Copyright Mattias Larsson Sköld 2019
#pragma once

#include "main/merror.h"
#include <map>
#include <set>
#include <string>

enum class CompilerString {
    IncludePrefix,
    SystemIncludePrefix,
    DefinePrefix,
    PICFlag,
    SharedFileEnding,
    StaticFileEnding,
    RPathOriginFlag,
    BuildOutputPath,
    CompileFile,
};

enum class CompilerFlagType { RequiresPICForLibrary };

class ICompiler {
public:
    typedef CompilerString CS;
    typedef CompilerFlagType CF;

    virtual ~ICompiler() = default;
    virtual std::string getString(CompilerString) = 0;
    virtual bool getFlag(CompilerFlagType) = 0;
    virtual std::string translateConfig(Token) = 0;
    virtual std::string prepareLinkString(std::string dir,
                                          std::string name) = 0;

    virtual std::string getPrecompiledModuleFlags(
        std::set<std::string> paths) = 0;
};

class GCCCompiler : public ICompiler {
    std::string getString(CompilerString type) override {
        switch (type) {
        case CS::IncludePrefix:
            return "-I";
        case CS::SystemIncludePrefix:
            return "-isystem ";
        case CS::PICFlag:
            return "-fPIC ";
        case CS::DefinePrefix:
            return "-D";
        case CS::SharedFileEnding:
            return ".so";
        case CS::StaticFileEnding:
            return ".a";
        case CS::RPathOriginFlag:
            return "-Wl,-rpath='${ORIGIN}'";
        case CS::BuildOutputPath:
            return "-o";
        case CS::CompileFile:
            return "-c";
        }
        return {};
    }

    bool getFlag(CompilerFlagType flag) override {
        switch (flag) {
        case CF::RequiresPICForLibrary:
            return true;
        }
        return false;
    }

    std::string prepareLinkString(std::string dir, std::string name) override {
        return "-l:" + name + " -L " + dir;
    }

    std::string translateConfig(Token name) override {
        const std::map<std::string, std::string> translateMap = {
            {"Wall", "-Wall"},
            {"debug", "-g"},
            {"modules", "-fmodules-ts"}, // Actually for clang... well well
            {"threads", "-pthread"},
            {"filesystem", "-lstdc++fs"},
        };

        if (name.substr(0, 3) == "c++") {
            return "-std=" + name;
        }

        try {
            return translateMap.at(name);
        }
        catch (std::out_of_range &) {
            throw MatmakeError(name, "Config not found");
            return "";
        }
    }

    std::string getPrecompiledModuleFlags(
        std::set<std::string> paths) override {
        std::string ret;
        for (auto &path : paths) {
            ret += (" -fprebuilt-module-path=" + path);
        }

        return ret + " ";
    }
};

typedef GCCCompiler ClangCompiler;

class MSVCCompiler : public ICompiler {
    std::string getString(CompilerString type) override {
        switch (type) {
        case CS::IncludePrefix:
            return "/I ";
        case CS::SystemIncludePrefix:
            return "/I ";
        case CS::PICFlag:
            return "";
        case CS::DefinePrefix:
            return "/D";
        case CS::SharedFileEnding:
            return ".dll";
        case CS::StaticFileEnding:
            return ".lib";
        case CS::RPathOriginFlag:
            return "";
        case CS::BuildOutputPath:
            return "/out:";
        case CS::CompileFile:
            return "/c";
        }
        return {};
    }

    bool getFlag(CompilerFlagType flag) override {
        switch (flag) {
        case CF::RequiresPICForLibrary:
            return true;
        }
        return false;
    }

    std::string prepareLinkString(std::string /*dir*/,
                                  std::string name) override {
        return "lib?" + name;
    }

    std::string getPrecompiledModuleFlags(
        std::set<std::string> /*paths*/) override {
        return {};
    }
};

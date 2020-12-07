// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "compilertype.h"
#include "dependency/buildfile.h"
#include "dependency/copyfile.h"
#include "dependency/dependency.h"
#include "dependency/linkfile.h"
#include "environment/files.h"
#include "environment/globals.h"
#include "environment/ifiles.h"
#include "target/ibuildtarget.h"
#include "target/targetproperties.h"
#include "targets.h"
#include <map>
#include <set>

//! A build target is a executable, dll or similar that depends
//! on one or more build targets, build files or copy files
struct BuildTarget : public IBuildTarget {
    //    std::map<Token, Tokens> _properties;
    Token _name;

    std::shared_ptr<ICompiler> _compilerType = std::make_shared<GCCCompiler>();
    LinkFile *_outputFile = nullptr;
    bool _isBuildCalled = false;
    bool _hasModules = false;
    std::unique_ptr<TargetProperties> _properties;
    std::set<std::string> _precompilePaths;

    BuildTarget(std::unique_ptr<TargetProperties> properties) {
        _properties = move(properties);
        _name = _properties->name();
        _hasModules = _properties->hasConfig("modules");
    }

    const TargetProperties &properties() const override {
        return *_properties;
    }

    Token getLibs() const override {
        auto libs = properties().get("libs");

        return libs.concat();
    }

    Token preprocessCommand(Token command) const override {
        for (size_t find = command.find('%'); find != std::string::npos;
             find = command.find('%', find + 1)) {
            command.replace(find, 1, name());
        }

        return command;
    }

    Token getFlags() const override {
        return properties().get("flags").concat() + " " + getConfigFlags();
    }

    Token getIncludeFlags() const {
        Token ret;

        auto includes = properties().get("includes").groups();
        for (auto &include : includes) {
            auto includeStr = include.concat();
            if (includeStr.empty()) {
                continue;
            }
            ret +=
                (" " + _compilerType->getString(CompilerString::IncludePrefix) +
                 includeStr);
        }

        auto sysincludes = properties().get("sysincludes").groups();
        for (auto &include : sysincludes) {
            auto includeStr = include.concat().trim();
            if (includeStr.empty()) {
                continue;
            }
            ret +=
                (" " +
                 _compilerType->getString(CompilerString::SystemIncludePrefix) +
                 include.concat());
        }

        return ret;
    }

    Token getDefineFlags() const {
        Token ret;

        auto defines = properties().get("define").groups();
        for (auto &def : defines) {
            ret +=
                (" " + _compilerType->getString(CompilerString::DefinePrefix) +
                 def.concat());
        }

        return ret;
    }

    Token getConfigFlags() const {
        Token ret;
        auto configs = properties().get("config").groups();
        for (auto &config : configs) {
            ret += (" " + _compilerType->translateConfig(config.concat()));
        }
        return ret;
    }

    //! Returns all files in a property
    Tokens getGroups(const Token &propertyName, const IFiles &files) const {
        auto sourceString = properties().get(propertyName);

        auto groups = sourceString.groups();

        Tokens ret;
        for (auto g : groups) {
            auto sourceFiles = files.findFiles(g.concat());
            ret.insert(ret.end(), sourceFiles.begin(), sourceFiles.end());
        }

        if (ret.size() == 1) {
            if (ret.front().empty()) {
                vout << " no pattern matching for " << propertyName << "\n";
            }
        }
        return ret;
    }

    void print() const override {
        vout << "target " << _name << ": \n";
        for (auto &m : properties().properties()) {
            vout << "\t" << m.first << " = " << m.second << " \n";
        }
        vout << "\n";
    }

    Token getCompiler(const Token &filetype) const override {
        if (filetype == "cpp" || filetype == "cppm") {
            return properties().get("cpp").concat();
        }
        else if (filetype == "a") {
            return properties().get("ar").concat();
        }
        else if (filetype == "c") {
            return properties().get("cc").concat();
        }
        else {
            return "echo";
        }
    }

    BuildRuleList calculateDependencies(const IFiles &files,
                                        const Targets &targets) override {
        if (name() == "root") {
            return {};
        }

        BuildRuleList dependencies;

        auto oFile =
            std::make_unique<LinkFile>(filename(), this, _compilerType.get());
        _outputFile = oFile.get();

        for (auto filename : getGroups("src", files)) {
            if (filename.empty()) {
                continue;
            }
            filename = preprocessCommand(filename);
            auto ending = stripFileEnding(filename).second;
            if (_hasModules && ending == "cppm") {
                dependencies.push_back(std::make_unique<BuildFile>(
                    filename, this, BuildFile::CppToPcm, _compilerType.get()));
                _precompilePaths.insert(
                    getDirectory(dependencies.back()->dependency().output()));

                dependencies.push_back(std::make_unique<BuildFile>(
                    filename, this, BuildFile::PcmToO, _compilerType.get()));
            }
            else {
                dependencies.push_back(std::make_unique<BuildFile>(
                    filename, this, BuildFile::CppToO, _compilerType.get()));
            }
        }
        for (auto &filename : getGroups("copy", files)) {
            if (filename.empty()) {
                continue;
            }
            filename = preprocessCommand(filename);
            dependencies.push_back(std::make_unique<CopyFile>(filename, this));
        }
        for (auto &link : getGroups("link", files)) {
            if (link.empty()) {
                continue;
            }
            link = preprocessCommand(link);
            auto dependencyTarget = targets.find(link);
            if (dependencyTarget && dependencyTarget->outputFile()) {
                _outputFile->dependency().addDependency(
                    dependencyTarget->outputFile());
            }
            else {
                throw MatmakeError(link, " Could not find target " + link);
            }
        }

        for (auto &dep : dependencies) {
            if (dep->dependency().includeInBinary()) {
                _outputFile->dependency().addDependency(&dep->dependency());
            }
        }

        dependencies.push_back(move(oFile));

        auto configs = properties().get("config");
        for (const auto &config : configs) {
            if (config == "modules") {
                _hasModules = true;
            }
        }

        return dependencies;
    }

    bool hasModules() const override {
        return _hasModules;
    }

    BuildType buildType() const override {
        auto out = properties().get("out");
        if (!out.empty()) {
            if (out.front() == "shared") {
                return Shared;
            }
            else if (out.front() == "static") {
                return Static;
            }
            else if (out.front() == "test") {
                return Test;
            }
        }
        return Executable;
    }

    //! Path minus directory
    Token filename() const override {
        auto out = properties().get("out").groups();
        if (out.empty()) {
            return _name;
        }
        else if (out.size() == 1) {
            return preprocessCommand(out.front().concat());
        }
        else if (out.size() > 1) {
            auto type = out.front().concat();
            auto striped =
                stripFileEnding(preprocessCommand(out[1].concat()), true);
            auto outName = striped.first;

            if (type == "shared") {
                return outName + _compilerType->getString(
                                     CompilerString::SharedFileEnding);
            }
            else if (type == "static") {
                return outName + _compilerType->getString(
                                     CompilerString::StaticFileEnding);
            }
            else if (type == "exe") {
                return outName;
            }
            else if (type == "test") {
                return outName;
            }
            else {
                throw MatmakeError(type, "Unknown type " + type);
            }
        }
        throw std::runtime_error("this should never happend");
    }

    Token name() const override {
        return _name;
    }

    //! Where the final product will be placed
    Token getOutputDir() const override {
        auto outputDir = properties().get("dir").concat();
        if (!outputDir.empty()) {
            outputDir = outputDir.trim();
            outputDir += "/";
        }
        return outputDir;
    }

    //! If where the tmp build-files is placed
    Token getBuildDirectory() const override {
        auto outputDir = properties().get("objdir").concat();
        if (!outputDir.empty()) {
            outputDir = outputDir.trim();
            outputDir += "/";
        }
        else {
            return getOutputDir();
        }
        return outputDir;
    }

    //! Return flags used by a file
    virtual Token getBuildFlags(const Token &filetype) const override {
        auto flags = properties().get("flags").concat();
        if (filetype == "cpp" || filetype == "cppm") {
            auto cppflags = properties().get("cppflags");
            if (!cppflags.empty()) {
                flags += (" " + cppflags.concat());
            }
        }
        if (filetype == "c") {
            auto cflags = properties().get("cflags");
            if (!cflags.empty()) {
                flags += (" " + cflags.concat());
            }
        }

        flags += getDefineFlags();

        flags += getConfigFlags();

        flags += getIncludeFlags();
        auto buildType = this->buildType();
        if (buildType == Shared) {
            if (_compilerType->getFlag(
                    CompilerFlagType::RequiresPICForLibrary)) {
                flags +=
                    (" " + _compilerType->getString(CompilerString::PICFlag));
            }
        }

        flags += _compilerType->getPrecompiledModuleFlags(_precompilePaths);

        // Todo: optimize performance
        //		return (_buildFlags = flags);
        return flags;
    }

    IDependency *outputFile() const override {
        if (_outputFile) {
            return &_outputFile->dependency();
        }
        else {
            return nullptr;
        }
    }
};

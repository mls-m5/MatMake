// Copyright Mattias Larsson Sköld 2019

#pragma once

#include "compilertype.h"
#include "dependency/buildfile.h"
#include "dependency/copyfile.h"
#include "dependency/dependency.h"
#include "dependency/linkfile.h"
#include "environment/globals.h"
#include "environment/ifiles.h"
#include "target/ibuildtarget.h"
#include "targets.h"

#include <map>

//! A build target is a executable, dll or similar that depends
//! on one or more build targets, build files or copy files
struct BuildTarget : public IBuildTarget {
    std::map<Token, Tokens> _properties;
    Token _name;

    std::shared_ptr<ICompiler> _compilerType = std::make_shared<GCCCompiler>();
    LinkFile *_outputFile = nullptr;
    bool _isBuildCalled = false;

    BuildTarget(Token name, IBuildTarget *root) : _name(name) {
        if (_name != "root") {
            inherit(root);
        }
        else {
            assign("cpp", Token("c++"));
            assign("cc", Token("cc"));
            assign("includes", Token(""));
        }
    }

    BuildTarget(IBuildTarget *root) {
        inherit(root);
    }

    BuildTarget(NameDescriptor n, Tokens v, IBuildTarget *root)
        : BuildTarget(n.rootName, root) {
        assign(n.propertyName, v);
    }

    void inherit(const IBuildTarget *parent) {
        if (!parent) {
            return;
        }
        for (auto v : parent->properties()) {
            if (v.first == "inherit") {
                continue;
            }
            assign(v.first, v.second);
        }
    }

    Tokens &property(Token propertyName) override {
        return _properties[propertyName];
    }

    void assign(Token propertyName, Tokens value) override {
        if (propertyName == "inherit") {
            std::cout << "Target " << name() << " tries to inherit wrong\n";
        }
        property(propertyName) = value;
    }

    void assign(Token propertyName,
                Tokens value,
                const Targets &targets) override {
        property(propertyName) = value;

        if (propertyName == "inherit") {
            auto parent = targets.find(value.concat());
            if (parent) {
                inherit(parent);
            }
        }
    }

    void append(Token propertyName, Tokens value) override {
        property(propertyName).append(value);
    }

    Tokens get(const Token &propertyName) const override {
        try {
            return properties().at(propertyName);
        }
        catch (std::out_of_range &) {
            return {};
        }
    }

    const std::map<Token, Tokens> &properties() const override {
        return _properties;
    }

    Token getLibs() const override {
        auto libs = get("libs");

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
        return get("flags").concat() + " " + getConfigFlags();
    }

    Token getIncludeFlags() const {
        Token ret;

        auto includes = get("includes").groups();
        for (auto &include : includes) {
            auto includeStr = include.concat();
            if (includeStr.empty()) {
                continue;
            }
            ret +=
                (" " + _compilerType->getString(CompilerString::IncludePrefix) +
                 includeStr);
        }

        auto sysincludes = get("sysincludes").groups();
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

        auto defines = get("define").groups();
        for (auto &def : defines) {
            ret +=
                (" " + _compilerType->getString(CompilerString::DefinePrefix) +
                 def.concat());
        }

        return ret;
    }

    Token getConfigFlags() const {
        Token ret;
        auto configs = get("config").groups();
        for (auto &config : configs) {
            ret += (" " + _compilerType->translateConfig(config.concat()));
        }
        return ret;
    }

    //! Returns all files in a property
    Tokens getGroups(const Token &propertyName, const IFiles &files) const {
        auto sourceString = get(propertyName);

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

    void print() override {
        vout << "target " << _name << ": \n";
        for (auto &m : properties()) {
            vout << "\t" << m.first << " = " << m.second << " \n";
        }
        vout << "\n";
    }

    Token getCompiler(const Token &filetype) const override {
        if (filetype == "cpp") {
            return get("cpp").concat();
        }
        else if (filetype == "c") {
            return get("cc").concat();
        }
        else {
            return "echo";
        }
    }

    std::vector<std::unique_ptr<IDependency>> calculateDependencies(
        const IFiles &files, const Targets &targets) override {
        if (name() == "root") {
            return {};
        }

        std::vector<std::unique_ptr<IDependency>> dependencies;

        auto oFile =
            std::make_unique<LinkFile>(filename(), this, _compilerType.get());
        _outputFile = oFile.get();

        for (auto filename : getGroups("src", files)) {
            if (filename.empty()) {
                continue;
            }
            filename = preprocessCommand(filename);
            dependencies.emplace_back(new BuildFile(filename, this));
        }
        for (auto &filename : getGroups("copy", files)) {
            if (filename.empty()) {
                continue;
            }
            filename = preprocessCommand(filename);
            dependencies.emplace_back(new CopyFile(filename, this));
        }
        for (auto &link : getGroups("link", files)) {
            if (link.empty()) {
                continue;
            }
            link = preprocessCommand(link);
            auto dependencyTarget = targets.find(link);
            if (dependencyTarget && dependencyTarget->outputFile()) {
                _outputFile->addDependency(dependencyTarget->outputFile());
            }
            else {
                throw MatmakeError(link, " Could not find target " + link);
            }
        }

        for (auto &dep : dependencies) {
            if (dep->includeInBinary()) {
                _outputFile->addDependency(dep.get());
            }
        }

        dependencies.push_back(move(oFile));

        return dependencies;
    }

    BuildType buildType() const override {
        auto out = get("out");
        if (!out.empty()) {
            if (out.front() == "shared") {
                return Shared;
            }
            else if (out.front() == "static") {
                return Static;
            }
        }
        return Executable;
    }

    //! Path minus directory
    Token filename() override {
        auto out = get("out").groups();
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
        auto outputDir = get("dir").concat();
        if (!outputDir.empty()) {
            outputDir = outputDir.trim();
            outputDir += "/";
        }
        return outputDir;
    }

    //! If where the tmp build-files is placed
    Token getBuildDirectory() const override {
        auto outputDir = get("objdir").concat();
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
        auto flags = get("flags").concat();
        if (filetype == "cpp") {
            auto cppflags = get("cppflags");
            if (!cppflags.empty()) {
                flags += (" " + cppflags.concat());
            }
        }
        if (filetype == "c") {
            auto cflags = get("cflags");
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

        // Todo: optimize performance
        //		return (_buildFlags = flags);
        return flags;
    }

    IDependency *outputFile() const override {
        return _outputFile;
    }
};
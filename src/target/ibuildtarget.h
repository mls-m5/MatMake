#pragma once

#include "dependency/buildtype.h"
#include "dependency/ibuildrule.h"
#include "environment/ifiles.h"
#include "main/token.h"
#include "target/targetproperties.h"
#include <map>
#include <memory>
#include <vector>

//! A build target is a collection of properties used te create a executable
//! build targets can "inherit" from eachother, and thereby share common
//! properties
class IBuildTarget {
public:
    virtual ~IBuildTarget() = default;

    //------------------ Property handling ------------------------------------

    virtual const TargetProperties &properties() const = 0;

    //-------- Values made from combining the buildtargets properties ---------

    //! Where the final product will be placed
    virtual Token getOutputDir() const = 0;

    //! Where temporary object files will be placed
    virtual Token getBuildDirectory() const = 0;

    virtual Token getCompiler(const Token &filetype) const = 0;

    virtual Token getBuildFlags(const Token &filetype) const = 0;

    virtual class IDependency *outputFile() const = 0;

    //! Replace variables in commands with strings
    //! For example replaces % with target name
    virtual Token preprocessCommand(Token command) const = 0;

    virtual Token name() const = 0;

    virtual Token getLibs() const = 0;

    virtual Token getFlags() const = 0;

    virtual BuildType buildType() const = 0;

    virtual bool hasModules() const = 0;

    // ------- Commands that is executed on the build target -------------------

    //! Calculate and return all files used by this target
    virtual std::vector<std::unique_ptr<IBuildRule>> calculateDependencies(
        const IFiles &files, const class Targets &targets) = 0;

    virtual void print() const = 0;

    virtual Token filename() const = 0;
};

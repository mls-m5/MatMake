#pragma once

#include "buildtype.h"
#include "ifiles.h"
#include "token.h"
#include <map>
#include <memory>
#include <vector>

class IBuildTarget {
public:
    virtual ~IBuildTarget() = default;

    //------------------ Property handling ------------------------------------

    //! Gets a list of const properties to iterate over
    virtual const std::map<Token, Tokens> &properties() const = 0;

    //! Get a single editable property
    virtual Tokens &property(Token) = 0;

    //! Get but without reference
    //! (This may very well be redundant
    virtual Tokens get(const Token &propertyName) const = 0;

    //! Used for setting _and handling_ properties on a build target
    //! This handles for example "inherit" or "exe" rules in the right way
    virtual void assign(Token propertyName,
                        Tokens value,
                        const class Targets &) = 0;
    virtual void assign(Token propertyName, Tokens value) = 0;

    //! Adding extra data to a variable (existing or non-existing)
    virtual void append(Token propertyName, Tokens value) = 0;

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

    // ------- Commands that is executed on the build target -------------------

    //! Calculate and return all files used by this target
    virtual std::vector<std::unique_ptr<class IDependency>>
    calculateDependencies(const IFiles &files,
                          const class Targets &targets) = 0;

    //	virtual void build() = 0;

    virtual void print() = 0;

    //	virtual void clean() = 0;

    virtual Token filename() = 0;
};

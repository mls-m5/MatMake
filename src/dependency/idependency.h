#pragma once

#include "buildtype.h"
#include "environment/ifiles.h"
#include "main/token.h"
#include "target/ibuildtarget.h"
#include <set>

class IDependency {
public:
    virtual ~IDependency() = default;

    // ----------- Higher level functions used as build steps -----------------

    //! For c++20 modules create a .d-file containing all dependencies
    //! For dependencies without modules enabled .d-file is created in
    //! "prepare"-step
    virtual void prescan(
        IFiles &files,
        const std::vector<std::unique_ptr<IDependency>> &buildFiles) = 0;

    //! Check if the file is dirty and setup build command
    virtual void prepare(const IFiles &files) = 0;

    //! Transform the source file to the output file
    //! Example do the compiling, copying or linking
    [[nodiscard]] virtual std::string work(const IFiles &files,
                                           class ThreadPool &pool) = 0;

    //! Remove all output files
    virtual void clean(const IFiles &files) = 0;

    //! Remove fresh dependencies from dependency tree
    virtual void prune() = 0;

    // -------------------------- Other functions -----------------------------

    // Sets and gets the state of the output file
    virtual void dirty(bool) = 0;
    virtual bool dirty() const = 0;

    //! Get the time when the file was latest changed
    //! 0 means that the file is not built at all
    virtual time_t changedTime(const IFiles &files) const = 0;

    //! Tell a dependency that the built of this file is finished
    //! Set pruned to true if
    virtual void notice(IDependency *d, class ThreadPool &pool) = 0;

    //! The path to where the target will be built
    virtual Token output() const = 0;

    //! The main target and implicit targets (often dependency files)
    virtual const std::vector<Token> &outputs() const = 0;

    //! A subscriber is a dependency that want a notice when the file is built
    virtual void addSubscriber(IDependency *s) = 0;

    //! Add a file that this file will wait for
    virtual void addDependency(IDependency *file) = 0;

    //! If the file should be used in the link step
    //! This should be false for pcm files or copied resource files
    virtual bool includeInBinary() const = 0;

    virtual BuildType buildType() const = 0;

    virtual Token linkString() const = 0;

    virtual const std::set<class IDependency *> dependencies() const = 0;

    virtual const IBuildTarget *target() const = 0;
};

#pragma once

#include "environment/ifiles.h"
#include <memory>
#include <optional>

class IDependency;

class IBuildRule {
public:
    //! For c++20 modules create a .d-file containing all dependencies
    //! For dependencies without modules enabled .d-file is created in
    //! "prepare"-step
    virtual void prescan(
        IFiles &files,
        const std::vector<std::unique_ptr<IBuildRule>> &buildFiles) = 0;

    //! Check if the file is dirty and setup build command
    virtual void prepare(const IFiles &files) = 0;

    //! Transform the source file to the output file
    //! Example do the compiling, copying or linking
    [[nodiscard]] virtual std::string work(const IFiles &files,
                                           class IThreadPool &pool) = 0;

    //    //! Remove all output files
    //    virtual void clean(const IFiles &files) = 0;

    virtual IDependency &dependency() = 0;
};

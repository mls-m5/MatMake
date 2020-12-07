#pragma once

#include "environment/ifiles.h"
#include <memory>
#include <string>

class IDependency;

using BuildRuleList = std::vector<std::unique_ptr<class IBuildRule>>;

class IBuildRule {
public:
    virtual ~IBuildRule() = default;

    //! For c++20 modules create a .d-file containing all dependencies
    //! For dependencies without modules enabled .d-file is created in
    //! "prepare"-step
    virtual void prescan(IFiles &, const BuildRuleList &) = 0;

    //! Check if the file is dirty and setup build command
    virtual void prepare(const IFiles &files, BuildRuleList &) = 0;

    //! Transform the source file to the output file
    //! Example do the compiling, copying or linking
    [[nodiscard]] virtual std::string work(const IFiles &files,
                                           class IThreadPool &pool) = 0;

    virtual IDependency &dependency() = 0;

    //! Specific for c++ modules
    //! Return empty string if not a module
    virtual std::string moduleName() const {
        return {};
    }
};

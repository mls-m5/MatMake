// Copyright Mattias Larsson Sköld

#pragma once

#include "dependency/dependency.h"
#include "dependency/ibuildrule.h"
#include "environment/globals.h"
#include "environment/ienvironment.h"
#include "target/ibuildtarget.h"

class CopyFile : public IBuildRule {
    std::unique_ptr<IDependency> _dep;

public:
    CopyFile(const CopyFile &) = delete;
    CopyFile(CopyFile &&) = delete;

    //! @param dependency: whine null a new dependency is created
    //!                    otherwise it can be specifiede for mocking
    CopyFile(Token source,
             IBuildTarget *target,
             std::unique_ptr<IDependency> dependency = nullptr)
        : _dep(dependency
                   ? std::move(dependency)
                   : std::make_unique<Dependency>(target, false, Copy, this)) {
        auto o = joinPaths(target->getOutputDir(), source);
        if (o != source) {
            _dep->output(o);
        }
        else {
            dout << o << " does not need copying, same source and output\n";
        }
        _dep->input(source);
    }

    void prescan(IFiles &,
                 const std::vector<std::unique_ptr<IBuildRule>> &) override {}

    void prepare(const IFiles &files) override {
        if (_dep->output().empty()) {
            return;
        }
        if (_dep->output() == _dep->input()) {
            vout << "file " << _dep->output()
                 << " source and target is on same place. skipping\n";
        }

        if (_dep->inputChangedTime(files) > _dep->changedTime(files)) {
            _dep->dirty(true);
        }
    }

    std::string work(const IFiles &files, IThreadPool &pool) override {
        using namespace std;
        std::ostringstream ss;

        try {
            files.copyFile(_dep->input(), _dep->output());

            ss << "copy " << _dep->input() << " --> " << _dep->output() << endl;

            _dep->dirty(false);

            _dep->sendSubscribersNotice(pool);
        }
        catch (std::runtime_error &e) {
            std::cerr << ("could not copy file for target " +
                          _dep->target()->name() + "\n" + e.what())
                      << std::endl;
        }

        return ss.str();
    }

    IDependency &dependency() override {
        return *_dep;
    }
};

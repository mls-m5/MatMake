#pragma once

#include "mls-unit-test/mock.h"
#include "target/ibuildtarget.h"

class MockIBuildTarget : public IBuildTarget {
public:
    MOCK_METHOD(const TargetProperties &, properties, (), const override);

    MOCK_METHOD(Token, getOutputDir, (), const override);

    MOCK_METHOD(Token, getBuildDirectory, (), const override);

    MOCK_METHOD1(Token, getCompiler, (const Token &filetype), const override);

    MOCK_METHOD1(Token, getBuildFlags, (const Token &filetype), const override);

    MOCK_METHOD(IDependency *, outputFile, (), const override);

    MOCK_METHOD1(Token, preprocessCommand, (Token command), const override);

    MOCK_METHOD(Token, name, (), const override);

    MOCK_METHOD(Token, getLibs, (), const override);

    MOCK_METHOD(Token, getFlags, (), const override);

    MOCK_METHOD(BuildType, buildType, (), const override);

    MOCK_METHOD(bool, hasModules, (), const override);

    MOCK_METHOD2(std::vector<std::unique_ptr<IBuildRule>>,
                 calculateDependencies,
                 (const IFiles &files, const class Targets &targets),
                 override);

    MOCK_METHOD(void, print, (), const override);

    MOCK_METHOD(Token, filename, (), const override);
};

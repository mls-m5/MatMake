#pragma once

#include "mls-unit-test/mock.h"
#include "target/ibuildtarget.h"

class MockIBuildTarget : public IBuildTarget {
public:
    MOCK_METHOD0(const TargetProperties &, properties, (), const override);

    MOCK_METHOD0(Token, getOutputDir, (), const override);

    MOCK_METHOD0(Token, getBuildDirectory, (), const override);

    MOCK_METHOD1(Token, getCompiler, (const Token &filetype), const override);

    MOCK_METHOD1(Token, getBuildFlags, (const Token &filetype), const override);

    MOCK_METHOD0(IDependency *, outputFile, (), const override);

    MOCK_METHOD1(Token, preprocessCommand, (Token command), const override);

    MOCK_METHOD0(Token, name, (), const override);

    MOCK_METHOD0(Token, getLibs, (), const override);

    MOCK_METHOD0(Token, getFlags, (), const override);

    MOCK_METHOD0(BuildType, buildType, (), const override);

    MOCK_METHOD0(bool, hasModules, (), const override);

    MOCK_METHOD2(BuildRuleList,
                 calculateDependencies,
                 (const IFiles &files, const class Targets &targets),
                 override);

    MOCK_METHOD0(void, print, (), const override);

    MOCK_METHOD0(Token, filename, (), const override);
};

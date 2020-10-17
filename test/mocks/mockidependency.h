#pragma once

#include "dependency/dependency.h"
#include "mls-unit-test/mock.h"

class MockIDependency : public IDependency {
public:
    MOCK_METHOD2(std::string,
                 work,
                 (const IFiles &files, IThreadPool &pool),
                 override);
    MOCK_METHOD1(void, clean, (const IFiles &files), override);
    MOCK_METHOD0(void, prune, (), override);
    MOCK_METHOD1(void, dirty, (bool), override);
    MOCK_METHOD0(bool, dirty, (), const override);
    MOCK_METHOD1(time_t, changedTime, (const IFiles &files), const override);
    MOCK_METHOD1(time_t,
                 inputChangedTime,
                 (const IFiles &files),
                 const override);
    MOCK_METHOD2(void, notice, (IDependency * d, IThreadPool &pool), override);
    MOCK_METHOD0(Token, output, (), const override);
    MOCK_METHOD1(void, output, (Token value), override);
    MOCK_METHOD0(const std::vector<Token> &, outputs, (), const override);
    MOCK_METHOD1(void, addSubscriber, (IDependency * s), override);
    MOCK_METHOD1(void, sendSubscribersNotice, (IThreadPool & pool), override);
    MOCK_METHOD1(void, addDependency, (IDependency * file), override);
    MOCK_METHOD0(bool, includeInBinary, (), const override);
    MOCK_METHOD0(BuildType, buildType, (), const override);
    MOCK_METHOD1(void, linkString, (Token token), override);
    MOCK_METHOD0(Token, linkString, (), const override);
    MOCK_METHOD0(const std::set<IDependency *>,
                 dependencies,
                 (),
                 const override);
    MOCK_METHOD0(const IBuildTarget *, target, (), const override);
    MOCK_METHOD1(void, input, (Token in), override);
    MOCK_METHOD0(std::vector<Token>, inputs, (), const override);
    MOCK_METHOD0(Token, input, (), const override);
    MOCK_METHOD1(void, depFile, (Token file), override);
    MOCK_METHOD0(Token, depFile, (), const override);
    MOCK_METHOD0(Token, command, (), const override);
    MOCK_METHOD1(void, command, (Token command), override);
    MOCK_METHOD0(bool, shouldAddCommandToDepFile, (), const override);
    MOCK_METHOD1(void, shouldAddCommandToDepFile, (bool value), override);

    MOCK_METHOD0(IBuildRule *, parentRule, (), override);
    MOCK_METHOD1(void, parentRule, (IBuildRule *), override);
};

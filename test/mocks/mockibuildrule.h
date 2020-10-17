#pragma once

#include "dependency/ibuildrule.h"
#include "dependency/idependency.h"
#include "mls-unit-test/mock.h"

class MockIBuildRule : public IBuildRule {
public:
    MOCK_METHOD2(void, prescan, (IFiles &, const BuildRuleList &), override);

    MOCK_METHOD2(void, prepare, (const IFiles &, BuildRuleList &), override);

    MOCK_METHOD2(std::string,
                 work,
                 (const IFiles &, class IThreadPool &),
                 override);

    MOCK_METHOD0(IDependency &, dependency, (), override);

    MOCK_METHOD0(std::string, moduleName, (), const override);
};

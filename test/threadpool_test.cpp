
#include "environment/threadpool.h"
#include "mls-unit-test/unittest.h"
#include "mocks/mockibuildrule.h"
#include "mocks/mockidependency.h"
#include "mocks/mockifiles.h"

TEST_SUIT_BEGIN

TEST_CASE("work is dispatched to build rule") {
    ThreadPool pool;
    MockIDependency dependency;
    auto rule = std::make_unique<MockIBuildRule>();
    MockIFiles fileHandler;
    BuildRuleList fileList;

    dependency.mock_parentRule_0.expectMinNum(1);
    dependency.mock_parentRule_0.returnValue(rule.get());
    dependency.mock_dirty_0.nice();

    rule->mock_dependency_0.returnValueRef(dependency);
    rule->mock_work_2.expectNum(1);

    pool.addTask(&dependency);
    pool.addTaskCount();

    fileList.push_back(move(rule));
    pool.work(std::move(fileList), fileHandler);
}

TEST_SUIT_END


#include "dependency/copyfile.h"
#include "mls-unit-test/unittest.h"
#include "mocks/mockibuildtarget.h"
#include "mocks/mockidependency.h"
#include "mocks/mockifiles.h"
#include "mocks/mockithreadpool.h"

namespace {

auto createDependencyMock() {
    return std::make_unique<MockIDependency>();
}

struct TestFixture {
    MockIBuildTarget target;
    MockIFiles files;
    MockIThreadPool pool;
};

} // namespace

TEST_SUIT_BEGIN

TEST_CASE("instantiate") {
    TestFixture f;
    auto dep = createDependencyMock();

    dep->mock_output_1.expectArgs("bin/a.txt");
    dep->mock_input_1.expectArgs("a.txt");
    dep->mock_output_0.returnValue("a.txt");
    f.target.mock_getOutputDir_0.returnValue("bin");

    CopyFile copyFile("a.txt", &f.target, std::move(dep));
}

TEST_CASE("prepare") {
    TestFixture f;
    auto dep = createDependencyMock();

    dep->mock_input_1.nice();
    dep->mock_input_0.returnValue("a.txt");
    dep->mock_output_1.nice();
    dep->mock_output_0.returnValue("bin/a.txt");

    dep->mock_inputChangedTime_1.returnValue(10);
    dep->mock_changedTime_1.returnValue(7);

    f.target.mock_getOutputDir_0.returnValue("bin");

    dep->mock_dirty_1.expectArgs(true);

    CopyFile copyFile("a.txt", &f.target, std::move(dep));

    copyFile.prepare(f.files);
}

TEST_CASE("prepare (fresh)") {
    TestFixture f;
    auto dep = createDependencyMock();

    dep->mock_input_1.nice();
    dep->mock_input_0.returnValue("a.txt");
    dep->mock_output_1.nice();
    dep->mock_output_0.returnValue("bin/a.txt");

    dep->mock_inputChangedTime_1.returnValue(7);
    dep->mock_changedTime_1.returnValue(10);

    f.target.mock_getOutputDir_0.returnValue("bin");

    dep->mock_dirty_1.expectNum(0);

    CopyFile copyFile("a.txt", &f.target, std::move(dep));

    copyFile.prepare(f.files);
}

TEST_CASE("copy") {
    TestFixture f;
    auto dep = createDependencyMock();

    dep->mock_input_1.nice();
    dep->mock_input_0.returnValue("a.txt");
    dep->mock_output_1.nice();
    dep->mock_output_0.returnValue("bin/a.txt");

    dep->mock_inputChangedTime_1.returnValue(10);
    dep->mock_changedTime_1.returnValue(7);

    f.target.mock_getOutputDir_0.returnValue("bin");

    dep->mock_dirty_1.nice();

    auto rawDep = dep.get();

    CopyFile copyFile("a.txt", &f.target, std::move(dep));

    copyFile.prepare(f.files);

    rawDep->mock_dirty_1.expectArgs(false);

    f.files.mock_copyFile_2.expectArgs("a.txt", "bin/a.txt");
    rawDep->mock_sendSubscribersNotice_1.expectMinNum(1);

    copyFile.work(f.files, f.pool);
}

TEST_SUIT_END

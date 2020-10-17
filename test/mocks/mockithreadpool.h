#pragma once

#include "environment/ithreadpool.h"
#include "mls-unit-test/mock.h"

class MockIThreadPool : public IThreadPool {
public:
    MOCK_METHOD1(void, addTask, (IDependency *), override);
};

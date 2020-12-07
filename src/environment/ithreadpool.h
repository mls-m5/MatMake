#pragma once

class IThreadPool {
public:
    virtual ~IThreadPool() = default;

    virtual void addTask(class IDependency *t) = 0;
};

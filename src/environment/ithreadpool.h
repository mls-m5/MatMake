#pragma once

class IThreadPool {
public:
    virtual void addTask(class IDependency *t) = 0;
};

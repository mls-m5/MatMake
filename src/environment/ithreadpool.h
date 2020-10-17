#pragma once

class IThreadPool {
public:
    virtual void addTask(class IBuildRule *t) = 0;
};

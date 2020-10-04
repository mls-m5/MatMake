/*
 * threadpool.h
 *
 *  Created on: 1 feb. 2020
 *      Author: Mattias Larsson Sköld
 */

#pragma once

#include "idependency.h"
#include "ifiles.h"
#include "mdebug.h"
#include "merror.h"
#include <atomic>
#include <mutex>
#include <queue>
#include <vector>

class ThreadPool : std::queue<IDependency *> {
    std::mutex workMutex;
    std::mutex workAssignMutex;
    std::atomic<size_t> numberOfActiveThreads;
    int maxTasks = 0;
    int taskFinished = 0;
    int lastProgress = 0;

public:
    void addTask(IDependency *t) {
        workAssignMutex.lock();
        push(t);
        workAssignMutex.unlock();
        workMutex.try_lock();
        workMutex.unlock();
    }

    void addTaskCount() {
        ++maxTasks;
    }

    // This is what a single thread will do
    void workThreadFunction(int i, const IFiles &files) {
        using namespace std;

        dout << "starting thread " << endl;

        workAssignMutex.lock();
        while (!empty()) {

            auto t = front();
            pop();
            workAssignMutex.unlock();
            try {
                t->work(files, *this);
                stringstream ss;
                ss << "[" << getBuildProgress() << "%] ";
                vout << ss.str();
                ++this->taskFinished;
            }
            catch (MatmakeError &e) {
                cerr << e.what() << endl;
                globals.bailout = true;
            }
            printProgress();

            workAssignMutex.lock();
        }
        workAssignMutex.unlock();

        workMutex.try_lock();
        workMutex.unlock();

        dout << "thread " << i << " is finished quit" << endl;
        numberOfActiveThreads -= 1;
    }

    void workMultiThreaded(const IFiles &fileHandler) {
        using namespace std;
        vout << "running with " << globals.numberOfThreads << " threads"
             << endl;

        std::vector<std::thread> threads;
        numberOfActiveThreads = 0;

        auto f = [this, &fileHandler](int i) {
            workThreadFunction(i, fileHandler);
        };

        threads.reserve(globals.numberOfThreads);
        auto startTaskNum = size();
        for (size_t i = 0; i < globals.numberOfThreads && i < startTaskNum;
             ++i) {
            ++numberOfActiveThreads;
            threads.emplace_back(f, static_cast<int>(numberOfActiveThreads));
        }

        for (auto &t : threads) {
            t.detach();
        }

        while (numberOfActiveThreads > 0 && !globals.bailout) {
            workMutex.lock();
            dout << "remaining tasks " << size() << " tasks" << endl;
            dout << "number of active threads at this point "
                 << numberOfActiveThreads << endl;
            if (!empty()) {
                std::lock_guard<mutex> guard(workAssignMutex);
                auto numTasks = size();
                if (numTasks > numberOfActiveThreads) {
                    for (auto i = numberOfActiveThreads.load();
                         i < globals.numberOfThreads && i < numTasks;
                         ++i) {
                        dout << "Creating new worker thread to manage tasks"
                             << endl;
                        ++numberOfActiveThreads;
                        thread t(f, static_cast<int>(numberOfActiveThreads));
                        t.detach(); // Make the thread live without owning it
                    }
                }
            }
        }
    }

    void work(std::vector<std::unique_ptr<IDependency>> files,
              const IFiles &fileHandler) {
        using namespace std;
        if (globals.numberOfThreads > 1) {
            workMultiThreaded(fileHandler);
        }
        else {
            globals.numberOfThreads = 1;
            vout << "running with 1 thread" << endl;
            while (!empty()) {
                auto t = front();
                pop();
                try {
                    t->work(fileHandler, *this);
                }
                catch (MatmakeError &e) {
                    dout << e.what() << endl;
                    globals.bailout = true;
                }
            }
        }

        for (auto &file : files) {
            if (file->dirty()) {
                dout << "file " << file->output() << " was never built" << endl;
                dout << "depending on: " << endl;
                for (auto dependency : file->dependencies()) {
                    dependency->output();
                }
            }
        }

        vout << "finished" << endl;
    }

    int getBuildProgress() const {
        return (taskFinished * 100 / maxTasks);
    }

    void printProgress() {
        if (!maxTasks) {
            return;
        }
        int amount = getBuildProgress();

        if (amount == lastProgress) {
            return;
        }
        lastProgress = amount;

        std::stringstream ss;

        if (!globals.debugOutput && !globals.verbose) {
            ss << "[";
            for (int i = 0; i < amount / 4; ++i) {
                ss << "-";
            }
            if (amount < 100) {
                ss << ">";
            }
            else {
                ss << "-";
            }
            for (int i = amount / 4; i < 100 / 4; ++i) {
                ss << " ";
            }
            ss << "] " << amount << "%  \r";
        }
        else {
            // ss << "[" << amount << "%] ";
        }

        std::cout << ss.str();
        std::cout.flush();
    }
};

// Copyright Mattias Larsson Sköld

#pragma once

#include "files.h"
#include "globals.h"
#include "idependency.h"
#include "matmake-common.h"
#include "threadpool.h"
#include <chrono>
#include <fstream>
#include <mutex>
#include <set>

class IBuildTarget;

class Dependency : public IDependency {
    std::set<class IDependency *>
        _dependencies; // Dependencies from matmakefile
    std::set<IDependency *> _subscribers;
    std::mutex _accessMutex;
    bool _dirty = false;

    std::vector<Token> _outputs;
    std::vector<Token> _inputs;
    Token _command;
    Token _depFile;
    IBuildTarget *_target;

public:
    Dependency(IBuildTarget *target) : _target(target) {}

    virtual ~Dependency() override = default;

    // Get the latest time of times changed for input files
    virtual time_t inputChangedTime(const IFiles &files) const {
        time_t time = 0;

        for (auto &input : inputs()) {
            time = std::max(time, files.getTimeChanged(input));
        }

        return time;
    }

    //! Returns the changed time of the oldest of all output files
    virtual time_t changedTime(const IFiles &files) const override {
        time_t outputChangedTime = std::numeric_limits<time_t>::max();

        for (auto &out : _outputs) {
            auto changedTime = files.getTimeChanged(out);

            outputChangedTime = std::min(outputChangedTime, changedTime);
        }

        return outputChangedTime;
    }

    void addDependency(IDependency *file) override {
        if (file) {
            dout << "adding " << file->output() << " to " << output() << "\n";
            _dependencies.insert(file);
        }
    }

    Token output() const final {
        if (_outputs.empty()) {
            return "";
        }
        else {
            return _outputs.front();
        }
    }

    void clean(const IFiles &files) override {
        for (auto &out : outputs()) {
            if (std::find(_inputs.begin(), _inputs.end(), out) !=
                _inputs.end()) {
                continue; // Do not remove source files
            }
            vout << "removing file " << out << "\n";
            files.remove(out.c_str());
        }
    }

    bool includeInBinary() const override {
        return true;
    }

    void addSubscriber(IDependency *s) override {
        std::lock_guard<std::mutex> guard(_accessMutex);
        if (find(_subscribers.begin(), _subscribers.end(), s) ==
            _subscribers.end()) {
            _subscribers.insert(s);
        }
    }

    //! Send a notice to all subscribers
    virtual void sendSubscribersNotice(ThreadPool &pool) {
        std::lock_guard<std::mutex> guard(_accessMutex);
        for (auto s : _subscribers) {
            s->notice(this, pool);
        }
        _subscribers.clear();
    }

    //! A message from a object being subscribed to
    //! This is used by targets to know when all dependencies
    //! is built
    void notice(IDependency *d, ThreadPool &pool) override {
        _dependencies.erase(d);
        if (globals.debugOutput) {
            dout << "removing dependency " << d->output() << " from "
                 << output() << "\n";
            dout << "   " << _dependencies.size() << " remains ";
            for (auto d : _dependencies) {
                dout << d->output() << " " << std::endl;
            }
        }
        if (_dependencies.empty()) {
            pool.addTask(this);
            dout << "Adding " << output() << " to task list " << std::endl;
        }
    }

    void lock() {
        _accessMutex.lock();
    }

    void unlock() {
        _accessMutex.unlock();
    }

    static Token fixDepEnding(Token filename) {
        return filename + ".d";
    }

    //! Calculate dependencies from makefile styled .d files generated
    //! by the compiler
    std::pair<std::vector<std::string>, std::string> parseDepFile() const {
        using namespace std;
        ifstream file(target()->preprocessCommand(depFile()));
        if (file.is_open()) {
            vector<string> ret;
            string command;

            string line;
            bool firstLine = true;
            while (getline(file, line)) {
                istringstream ss(line);

                if (!line.empty() && line.front() == '\t') {
                    command = trim(line);
                    break;
                }
                else {
                    string d;
                    if (firstLine) {
                        ss >> d; // The first is the target path --> ignore
                        firstLine = false;
                    }
                    while (ss >> d) {
                        if (d !=
                            "\\") { // Skip backslash (is used before newlines)
                            ret.push_back(d);
                        }
                    }
                }
            }
            return {ret, command};
        }
        else {
            dout << "could not find .d file for " << output() << " --> "
                 << depFile() << endl;
            return {};
        }
    }

    bool dirty() const final {
        return _dirty;
    }
    void dirty(bool value) final {
        _dirty = value;
    }

    const std::set<class IDependency *> dependencies() const override {
        return _dependencies;
    }

    const std::set<IDependency *> &subscribers() const {
        return _subscribers;
    }

    const Token &command() const {
        return _command;
    }

    void command(Token command) {
        _command = std::move(command);
    }

    //! Set the primary output file for the dependency
    void output(Token output) {
        _outputs.insert(_outputs.begin(), output);
    }

    // Set which file to search for dependencies in
    // this file is also a implicit output file
    void depFile(Token file) {
        _depFile = file;
        _outputs.push_back(file);
    }

    Token depFile() const {
        return _depFile;
    }

    const std::vector<Token> &outputs() const override {
        return _outputs;
    }

    void inputs(std::vector<Token> in) {
        _inputs = std::move(in);
    }

    void input(Token in) {
        _inputs = {std::move(in)};
    }

    std::vector<Token> inputs() const {
        return _inputs;
    }

    Token input() const {
        if (_inputs.empty()) {
            return "";
        }
        else {
            return _inputs.front();
        }
    }

    Token linkString() const override {
        return output();
    }

    IBuildTarget *target() const override {
        return _target;
    }

    void work(const IFiles &files, ThreadPool &pool) override {
        using namespace std;

        if (!command().empty()) {
            vout << command() << endl;
            pair<int, string> res = files.popenWithResult(command());
            if (res.first) {
                throw MatmakeError(command(),
                                   "could not build object:\n" + command() +
                                       "\n" + res.second);
            }
            else {
                auto depFile = this->depFile();
                if (!depFile.empty() && files.getTimeChanged(depFile)) {
                    ofstream file(depFile, fstream::app);
                    file << "\t" << command();
                }

                if (!res.second.empty()) {
                    cout << (command() + "\n" + res.second + "\n")
                         << std::flush;
                }
            }
            dirty(false);
            sendSubscribersNotice(pool);
        }
    }

    void prune() override {
        dout << "pruning " << output() << std::endl;
        std::vector<IDependency *> toRemove;
        for (auto *dep : _dependencies) {
            if (!dep->dirty()) {
                toRemove.push_back(dep);
            }
        }
        for (auto d : toRemove) {
            dout << "removing dependency " << d->output() << std::endl;
            _dependencies.erase(d);
        }
    }
};

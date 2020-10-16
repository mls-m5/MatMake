// Copyright Mattias Larsson Sköld

#pragma once

#include "environment/files.h"
#include "environment/globals.h"
#include "environment/threadpool.h"
#include "idependency.h"
#include "main/matmake-common.h"
#include <chrono>
#include <mutex>
#include <set>

class IBuildTarget;

class Dependency : public virtual IDependency {
    std::set<class IDependency *>
        _dependencies; // Dependencies from matmakefile
    IBuildTarget *_target;
    std::set<IDependency *> _subscribers;
    std::mutex _accessMutex;
    bool _dirty = false;
    bool _shouldAddCommandToDepFile = false;
    bool _includeInBinary = true;
    BuildType _buildType = NotSpecified;

    std::vector<Token> _outputs;
    std::vector<Token> _inputs;
    Token _command;
    Token _depFile;
    Token _linkString;

public:
    Dependency(IBuildTarget *target, bool includeInBinary, BuildType type)
        : _target(target)
        , _includeInBinary(includeInBinary)
        , _buildType(type) {}

    virtual ~Dependency() override = default;

    // Get the latest time of times changed for input files
    time_t inputChangedTime(const IFiles &files) const override {
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
        return _includeInBinary;
    }

    void addSubscriber(IDependency *s) override {
        std::lock_guard<std::mutex> guard(_accessMutex);
        if (find(_subscribers.begin(), _subscribers.end(), s) ==
            _subscribers.end()) {
            _subscribers.insert(s);
        }
    }

    //! Send a notice to all subscribers
    void sendSubscribersNotice(ThreadPool &pool) override {
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

    //! If the work command does not provide command to the dep file
    //! if true the dep-file-command is added at the "work" stage
    bool shouldAddCommandToDepFile() const override {
        return _shouldAddCommandToDepFile;
    }

    void shouldAddCommandToDepFile(bool value) override {
        _shouldAddCommandToDepFile = value;
    }

    static Token fixDepEnding(Token filename) {
        return removeDoubleDots(filename + ".d");
    }

    //    //! Calculate dependencies from makefile styled .d files generated
    //    //! by the compiler
    //    //! first = dependencies, second = old command
    //    static std::pair<std::vector<std::string>, std::string> parseDepFile(
    //        Token depFile, const IFiles &files) {
    //        using namespace std;
    //        try {
    //            //            auto lines =
    //            // files.readLines(target()->preprocessCommand(depFile()));
    //            auto lines = files.readLines(depFile);
    //            vector<string> ret;
    //            string command;

    //            bool firstLine = true;
    //            for (auto line : lines) {
    //                istringstream ss(line);

    //                if (!line.empty() && line.front() == '\t') {
    //                    command = trim(line);
    //                    break;
    //                }
    //                else {
    //                    string d;
    //                    if (firstLine) {
    //                        ss >> d; // The first is the target path -->
    //                        ignore firstLine = false;
    //                    }
    //                    while (ss >> d) {
    //                        if (d !=
    //                            "\\") { // Skip backslash (is used before
    //                            newlines) ret.push_back(d);
    //                        }
    //                    }
    //                }
    //            }
    //            return {ret, command};
    //        }
    //        catch (std::runtime_error &e) {
    //            dout << "could not find .d file " << depFile << endl;
    //            return {};
    //        }
    //    }

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

    Token command() const override {
        return _command;
    }

    void command(Token command) override {
        _command = move(command);
    }

    //! Set the primary output file for the dependency
    void output(Token output) override {
        _outputs.insert(_outputs.begin(), output);
    }

    // Set which file to search for dependencies in
    // this file is also a implicit output file
    void depFile(Token file) override {
        _depFile = file;
        _outputs.push_back(file);
    }

    Token depFile() const override {
        return _depFile;
    }

    const std::vector<Token> &outputs() const override {
        return _outputs;
    }

    void inputs(std::vector<Token> in) {
        _inputs = move(in);
    }

    void input(Token in) override {
        _inputs = {move(in)};
    }

    void addInput(Token in) {
        _inputs.push_back(in);
    }

    std::vector<Token> inputs() const override {
        return _inputs;
    }

    Token input() const override {
        if (_inputs.empty()) {
            return "";
        }
        else {
            return _inputs.front();
        }
    }

    void linkString(Token token) override {
        _linkString = std::move(token);
    }

    Token linkString() const override {
        if (_linkString.empty()) {
            return output();
        }
        else {
            return _linkString;
        };
    }

    IBuildTarget *target() const override {
        return _target;
    }

    //! If the depfile has a command, otherwise it probably has only
    //! dependencies printed
    bool doesDepFileHasCommand(const IFiles &files) const {
        return files.parseDepFile(depFile()).second.empty();
    }

    std::string work(const IFiles &files, ThreadPool &pool) override {
        std::stringstream outputStream;

        if (!command().empty()) {
            outputStream << command() << "\n";
            std::pair<int, std::string> res = files.popenWithResult(command());
            if (res.first) {
                throw MatmakeError(command(),
                                   "could not build object:\n" + command() +
                                       "\n" + res.second);
            }
            else {
                bool overrideDepFileTime =
                    shouldAddCommandToDepFile() && doesDepFileHasCommand(files);

                auto depFile = this->depFile();
                if (!depFile.empty() &&
                    (files.getTimeChanged(depFile) || overrideDepFileTime)) {
                    files.appendToFile(depFile, "\t" + command());
                }

                if (!res.second.empty()) {
                    outputStream << command() + "\n" + res.second + "\n";
                }
            }
            dirty(false);
            sendSubscribersNotice(pool);
        }
        return outputStream.str();
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

    BuildType buildType() const override {
        if (_buildType == FromTarget) {
            return _target->buildType();
        }
        else {
            return _buildType;
        }
    }
};

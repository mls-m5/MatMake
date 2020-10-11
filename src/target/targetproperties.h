#pragma once

#include "main/matmake-common.h"
#include "main/token.h"
#include <iostream>
#include <map>
#include <memory>

//! Contains values aquired from parsing the Matmakefile
class TargetProperties {
public:
    //! Create a property instance, parent can be sett to null for when not
    //! inheriting from another object
    TargetProperties(Token name, const TargetProperties *parent) : _name(name) {
        inherit(parent);
    }

    //! Just return all properties
    const std::map<Token, Tokens> &properties() const {
        return _properties;
    }

    //! Get a single editable property
    Tokens &property(Token propertyName) {
        return _properties[propertyName];
    }

    //! Get but without reference
    //! (This may very well be redundant
    //! Does not fail if no property with the name is found
    Tokens get(const Token &propertyName) const {
        try {
            return properties().at(propertyName);
        }
        catch (std::out_of_range &) {
            return {};
        }
    }

    //! Return property, and create if it does not exist
    Tokens &operator[](const Token &propertyName) {
        return property(propertyName);
    }

    //! Used for setting _and handling_ properties on a build target
    //! This handles for example "inherit" or "exe" rules in the right way
    void assign(Token propertyName,
                Tokens value,
                const class TargetPropertyCollection &targets);

    void assign(Token propertyName, Tokens value) {
        if (propertyName == "inherit") {
            std::cout << "Target " << name() << " tries to inherit wrong\n";
        }
        property(propertyName) = value;
    }

    //! Adding extra data to a variable (existing or non-existing)
    void append(Token propertyName, Tokens value) {
        property(propertyName).append(value);
    }

    Token name() const {
        return _name;
    }

    friend class TargetPropertyCollection;

private:
    void inherit(const class TargetProperties *parent);

    void initRoot() {
        //! Set default compileres
        auto &cpp = property("cpp");
        if (cpp.empty()) {
            cpp = Token("c++");
        }

        //! Set default c compiler
        auto &cc = property("cc");
        if (cc.empty()) {
            cc = Token("cc");
        }

        auto &includes = property("includes");
        if (includes.empty()) {
            // Todo: I do not remember why i did this, consider removing
            includes = Token("");
        }
    }

    std::map<Token, Tokens> _properties;
    Token _name;
};

//! used to store and refer target properties to eachother
class TargetPropertyCollection
    : public std::vector<std::unique_ptr<TargetProperties>> {
private:
    TargetProperties *_root = nullptr;

public:
    TargetPropertyCollection() = default;
    TargetPropertyCollection(
        const std::map<std::string, std::vector<std::string>>
            &commandLineVars) {
        push_back(std::make_unique<TargetProperties>("root", nullptr));
        _root = front().get();
        setCommandLineVars(commandLineVars);
        _root->initRoot();
    }

    TargetPropertyCollection(const TargetPropertyCollection &) = delete;
    TargetPropertyCollection(TargetPropertyCollection &&) = default;
    TargetPropertyCollection &operator=(const TargetPropertyCollection &) =
        delete;
    TargetPropertyCollection &operator=(TargetPropertyCollection &&) = default;

    auto root() {
        return _root;
    }

    TargetProperties *find(const Token &name) const {
        if (name.empty()) {
            return nullptr;
        }
        auto it = std::find_if(
            begin(), end(), [&name](auto &t) { return t->name() == name; });

        if (it != end()) {
            return it->get();
        }

        return nullptr;
    }

    //! Find a target, or create it if it does not exist
    TargetProperties &operator[](Token name) {
        if (auto v = find(name)) {
            return *v;
        }
        else {
            emplace_back(std::make_unique<TargetProperties>(name, root()));
            return *back();
        }
    }

    void appendVariable(const NameDescriptor &name, Tokens value) {
        (*this)[name.rootName].append(name.propertyName, value);
    }

    void setVariable(const NameDescriptor &name, Tokens value) {
        (*this)[name.rootName].assign(name.propertyName, value, *this);
    }

private:
    //! Initiate root object with values from the command line
    void setCommandLineVars(
        const std::map<std::string, std::vector<std::string>> &vars) {
        for (auto element : vars) {
            for (auto var : element.second) {
                root()->append(element.first, Token(var));
            }
        }
    }
};

//! -------- Out of line definitions -----------------------

void TargetProperties::assign(Token propertyName,
                              Tokens value,
                              const TargetPropertyCollection &targets) {
    property(propertyName) = value;

    if (propertyName == "inherit") {
        auto parent = targets.find(value.concat());
        if (parent) {
            inherit(parent);
        }
    }
}

void TargetProperties::inherit(const TargetProperties *parent) {
    if (!parent) {
        return;
    }

    for (auto v : parent->properties()) {
        if (v.first == "inherit") {
            continue;
        }
        assign(v.first, v.second);
    }
}

// Copyright Mattias Larsson Sköld 2020

#pragma once

#include "ibuildtarget.h"

#include <vector>

// A container for all build targets
class Targets : public std::vector<std::unique_ptr<IBuildTarget>> {
public:
    IBuildTarget *root = nullptr;

    IBuildTarget *find(Token name) const {
        if (name.empty()) {
            return nullptr;
        }
        for (auto &v : *this) {
            if (v->name() == name) {
                return v.get();
            }
        }
        return nullptr;
    }
};

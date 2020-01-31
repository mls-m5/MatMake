// Copyright Mattias Larsson Sköld

#pragma once

#include "dependency.h"
#include "ibuildtarget.h"
#include "globals.h"
#include "ienvironment.h"
#include <fstream>

class CopyFile: public Dependency {
public:
	CopyFile(const CopyFile &) = delete;
	CopyFile(CopyFile &&) = delete;
	CopyFile(Token source, IBuildTarget *target, IEnvironment *envi):
			Dependency(envi, target) {
		auto o = joinPaths(target->getOutputDir(), source);
		if (o != source) {
			output(o);
		}
		else {
			dout << o << " does not need copying, same source and output" << endl;
		}
		input(source);
	}

	time_t getSourceChangedTime() {
		return env().fileHandler().getTimeChanged(input());
	}

	void build() override {
		if (output().empty()) {
			return;
		}
		if (output() == input()) {
			vout << "file " << output() << " source and target is on same place. skipping" << endl;
		}
		auto timeChanged = getTimeChanged();

		if (getSourceChangedTime() > timeChanged) {
			dirty(true);
		}
	}

	void work() override {
		ifstream src(input());
		if (!src.is_open()) {
			cout << "could not open file " << input() << " for copy for target " << target()->name() << endl;
		}

		ofstream dst(output());
		if (!dst) {
			cout << "could not open file " << output() << " for copy for target " << target()->name() << endl;
		}

		vout << "copy " << input() << " --> " << output() << endl;
		dst << src.rdbuf();

		dirty(false);

		sendSubscribersNotice();
	}

	bool includeInBinary() const override {
		return false;
	}

	BuildType buildType() const override {
		return Copy;
	}
};


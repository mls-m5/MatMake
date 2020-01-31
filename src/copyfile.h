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
	CopyFile(Token source, IBuildTarget *parent, IEnvironment *envi):
		  Dependency(envi),
		  source(source),
		  parent(parent) {
		auto o = joinPaths(parent->getOutputDir(), source);
		if (o != source) {
			output(o);
		}
		else {
			dout << o << " does not need copying, same source and output" << endl;
		}
	}

	Token source;
	IBuildTarget *parent;

	time_t getSourceChangedTime() {
		return env().fileHandler().getTimeChanged(source);
	}

	void build() override {
		if (output().empty()) {
			return;
		}
		if (output() == source) {
			vout << "file " << output() << " source and target is on same place. skipping" << endl;
		}
		auto timeChanged = getTimeChanged();

		if (getSourceChangedTime() > timeChanged) {
			dirty(true);
		}
	}

	void work() override {
		ifstream src(source);
		if (!src.is_open()) {
			cout << "could not open file " << source << " for copy for target " << parent->name() << endl;
		}

		ofstream dst(output());
		if (!dst) {
			cout << "could not open file " << output() << " for copy for target " << parent->name() << endl;
		}

		vout << "copy " << source << " --> " << output() << endl;
		dst << src.rdbuf();

		dirty(false);

		sendSubscribersNotice();
	}

	void clean() override {
		vout << "removing file " << output() << endl;
		if (output() != source) {
			// Extra redundant checks
			remove(output().c_str());
		}
	}
//
//	Token targetPath() const override {
//		return output;
//	}

	Token linkString() override {
		return output();
//		return targetPath();
	}

	bool includeInBinary() override {
		return false;
	}

	BuildType buildType() override {
		return Copy;
	}
};


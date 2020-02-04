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
	CopyFile(Token source, IBuildTarget *target):
			Dependency(target) {
		auto o = joinPaths(target->getOutputDir(), source);
		if (o != source) {
			output(o);
		}
		else {
			dout << o << " does not need copying, same source and output" << endl;
		}
		input(source);
	}

	void prepare(const IFiles &files) override {
		if (output().empty()) {
			return;
		}
		if (output() == input()) {
			vout << "file " << output() << " source and target is on same place. skipping" << endl;
		}

		if (inputChangedTime(files) > changedTime(files)) {
			dirty(true);
		}
	}

	void work(const IFiles &/*files*/, ThreadPool& pool) override {
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

		sendSubscribersNotice(pool);
	}

	bool includeInBinary() const override {
		return false;
	}

	BuildType buildType() const override {
		return Copy;
	}
};


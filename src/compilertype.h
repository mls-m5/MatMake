// Copyright Mattias Larsson Sköld 2019

#include <string>

enum class CompilerString {
	IncludePrefix,
	SystemIncludePrefix,
	DefinePrefix,
	PICFlag,
};

enum class CompilerFlagType {
	RequiresPICForLibrary
};

class ICompiler {
public:
	typedef CompilerString CS;
	typedef CompilerFlagType CF;

	virtual ~ICompiler() = default;
	virtual std::string getString(CompilerString) = 0;
	virtual bool getFlag(CompilerFlagType) = 0;
};

class GCCCompiler: public ICompiler {
	std::string getString(CompilerString type) override {
		switch (type) {
		case CS::IncludePrefix:
			return "-I";
		case CS::SystemIncludePrefix:
			return "-isystem ";
		case CS::PICFlag:
			return "-fPIC ";
		case CS::DefinePrefix:
			return "-D";
		}
		return {};
	}

	bool getFlag(CompilerFlagType flag) override {
		switch (flag) {
		case CF::RequiresPICForLibrary:
			return true;
		}
		return false;
	}
};

typedef GCCCompiler ClangCompiler;

class MSVCCompiler: public ICompiler {
	std::string getString(CompilerString type) override {
		switch (type) {
		case CS::IncludePrefix:
			return "/I ";
		case CS::SystemIncludePrefix:
			return "/I ";
		case CS::PICFlag:
			return "";
		case CS::DefinePrefix:
			return "/D";
		}
		return {};
	}

	bool getFlag(CompilerFlagType flag) override {
		switch (flag) {
		case CF::RequiresPICForLibrary:
			return true;
		}
		return false;
	}
};


// Copyright Mattias Larsson Sköld 2019

#include <string>
#include <map>

enum class CompilerString {
	IncludePrefix,
	SystemIncludePrefix,
	DefinePrefix,
	PICFlag,
	SharedFileEnding,
	StaticFileEnding,
	RPathOriginFlag,
	LibLinkFlag,
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
	virtual std::string translateConfig(std::string) = 0;
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
		case CS::SharedFileEnding:
			return ".so";
		case CS::StaticFileEnding:
			return ".a";
		case CS::RPathOriginFlag:
			return "-Wl,-rpath='${ORIGIN}'";
		case CS::LibLinkFlag:
			return "-l";
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

	std::string translateConfig(std::string name) override {
		const std::map<std::string, std::string> translateMap = {
			{"Wall", "-Wall"}
		};

		if (name.substr(0, 3) == "c++") {
			return "-std=" + name;
		}

		try {
			return translateMap.at(name);
		} catch (std::out_of_range) {
			return "";
		}
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
		case CS::SharedFileEnding:
			return ".dll";
		case CS::StaticFileEnding:
			return ".lib";
		case CS::RPathOriginFlag:
			return "";
		case CS::LibLinkFlag:
			return "?"; // Todo: Fix this when adding support for MSVC
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


/*
 * matmake.cpp
 *
 *  Created on: 9 apr. 2019
 *      Author: Mattias Lasersköld
 */


#include <cstdlib>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <map>
#include <dirent.h>
#include <set>
#include <cstdlib>
#include <ctime>



using namespace std;

#include <stdio.h> //For FILENAME_MAX

#ifdef WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

bool verbose = false;

//verbose output
#define vout if(verbose) cout

time_t getTimeChanged(const string &path) {
	struct stat file_stat;
	int err = stat(path.c_str(), &file_stat);
    if (err != 0) {
//        throw runtime_error(" [file_is_modified] stat");
    	vout << "could not get status of file " << path << endl;
    	return 0;
    }
    return file_stat.st_mtime;
}

#endif


// adapted from https://stackoverflow.com/questions/143174/how-do-i-get-the-directory-that-a-program-is-running-from
string getCurrentWorkingDirectory() {
	char currentPath[FILENAME_MAX];
	if (!GetCurrentDir(currentPath, sizeof(currentPath))) {
		throw runtime_error("could not get current working directory");
	}
	return currentPath;
}

// trim from both ends (copying)
static inline std::string trim(std::string s) {
	auto front = find_if(s.begin(), s.end(), [] (int ch) {
		return !isspace(ch);
	});
	auto back = find_if(s.rbegin(), s.rend(), [] (int ch) {
		return !isspace(ch);
	}).base();

    return string(front, back);
}



vector<string> listFiles(string directory) {
	vector<string> ret;
	DIR *dir = opendir(directory.c_str());
	struct dirent *ent;

	if (dir != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			string name = ent->d_name;
			if (name != "." && name != "..") {
				ret.emplace_back(name);
			}
		}
		closedir(dir);
	}
	else {
		throw runtime_error("could not open directory " + directory);
	}
	return ret;
}

string getDirectory(string filename) {
	auto directoryFound = filename.rfind("/");
	if (directoryFound != string::npos) {
		return string(filename.begin(), filename.begin() + directoryFound);
	}
	return filename;
}

vector<string> findFiles(string pattern) {
	pattern = trim(pattern);
	auto found = pattern.find('*');

	vector<string> ret;
	if (found != string::npos) {
		string beginning(pattern.begin(), pattern.begin() + found);
		string ending(pattern.begin() + found + 1, pattern.end());
		string directory = beginning;
		string fileNameBeginning;
		auto directoryEnding = directory.rfind('/');
		if (directoryEnding != string::npos && directoryEnding < found) {
			fileNameBeginning = string(directory.begin() + directoryEnding + 1, directory.begin() + found);
			directory = string(directory.begin(), directory.begin() + directoryEnding);
		}

		auto fileList = listFiles(beginning);
		for (auto &file: fileList) {
			auto endingPos = file.find(ending);
			if (endingPos == string::npos) {
				if (ending.empty()) {
					if (file.find(fileNameBeginning) == 0) {
						ret.push_back(directory + "/" + file);
					}
				}
			}
			else {
				if (endingPos == file.size() - ending.size() &&
						file.find(fileNameBeginning) == 0) {
					ret.push_back(directory + "/" + file);
				}
			}
		}
	}
	else {
		return {pattern};
	}
	return ret;
}

struct Token: public string {
	Token(const Token &t) = default;
	Token() = default;
	Token(Token && t) = default;
	Token &operator = (const Token &) = default;
	Token(const string str): string(str) {}
	Token(const char* c): string(c) {}


	friend std::ostream &operator << (std::ostream &s, const Token &t) {
		if (t.empty()) {
			s << "empty ";
		}
		s << (string)t << t.trailingSpace;
		return s;
	}

	string str() {
		return (string) *this;
	}

	string trailingSpace;
};

struct Tokens: public vector<Token> {
	Tokens() = default;
	Tokens(Tokens &&t) = default;
	Tokens(const Tokens &t) = default;
	Tokens(const Token &t) {
		emplace_back(t);
	}
	Tokens& operator=(const Tokens&) = default;
	Tokens(vector<Token>::iterator begin, vector<Token>::iterator end): vector<Token>(begin, end) {
	}

	friend std::ostream &operator << (std::ostream &s, const Tokens &t) {
		for (auto it = t.begin(); it != t.end(); ++it) {
			s << *it << it->trailingSpace;
		}
		return s;
	}

	//Find groups without spaces between them
	vector<Tokens> groups() {
		if (empty()) {
			return {};
		}

		vector<Tokens> ret;
		ret.emplace_back();

		for (auto &t: *this) {
			ret.back().emplace_back(t);
			if (!t.trailingSpace.empty()) {
				ret.emplace_back();
			}
		}
		return ret;
	}

	string concat() {
		string ret;
		for (auto it = begin(); it != end(); ++it) {
			ret += ((*it) + it->trailingSpace);
		}
		return ret;
	}

	Tokens &append (const Tokens &other) {
		if (!empty() && back().trailingSpace.empty()) {
			back().trailingSpace += " ";
		}
		insert(end(), other.begin(), other.end());
		return *this;
	}
};

struct NameDescriptor {
	NameDescriptor(std::vector<Token> name) {
		if (name.size() == 1) {
			memberName = name.front();
		}
		else if (name.size() == 3 && name[1] == ".") {
			rootName = name.front();
			memberName = name.back();
		}
	}

	NameDescriptor(const Token &name, const Token &memberName): rootName(name), memberName(memberName) {}

	bool empty() {
		return memberName.empty();
	}

	Token rootName = "root";
	Token memberName = "";
};

class Dependency {
public:
	set<class Dependency*> dependencies;
	bool dirty = false;

	virtual ~Dependency() {}


	time_t getTimeChanged() {
		return ::getTimeChanged(targetPath());
	}

	virtual time_t build() = 0;

	void addDependency(Dependency *file) {
		dependencies.insert(file);
	}

	virtual string targetPath() = 0;

	virtual void clean() = 0;

	virtual bool includeInBinary() { return true; };
};

struct StaticFile: public Dependency {
	string filename;

	string targetPath() override {
		return filename;
	}

	time_t build() override {
		return getTimeChanged();
	}

	void clean() override {
	}

};

struct BuildTarget: public Dependency {
	Token name;
	map<Token, Tokens> members;
	class Environment *env;

	BuildTarget(Token name, class Environment *env): name(name), env(env){
		if (name != "root") {
			set("inherit", Token("root"));
		}
		else {
			set("exe", Token("program"));
			set("cpp", Token("c++"));
		}
	};

	BuildTarget(class Environment *env): env(env){
		set("inherit", Token("root"));
	};

	BuildTarget(NameDescriptor n, Tokens v, Environment *env): BuildTarget(n.rootName, env) {
		set(n.memberName, v);
	}

	void inherit(const BuildTarget &parent) {
		for (auto v: parent.members) {
			if (v.first == "inherit") {
				continue;
			}
			set(v.first, v.second);
		}
	}


	void set(Token memberName, Tokens value) {
		members[memberName] = value;

		if (memberName == "inherit") {
			auto parent = getParent();
			if (parent) {
				inherit(*parent);
			}
		}
	}

	void append(Token memberName, Tokens value) {
		members[memberName].append(value);
	}

	Tokens get(const NameDescriptor &name) {
		return get(name.memberName);
	}

	Tokens get(const Token &memberName) {
		try {
			return members.at(memberName);
		}
		catch (out_of_range &e) {
			return {};
		}
	}

	Tokens getGroups(const Token &memberName) {
		auto sourceString = get(memberName);

		auto groups = sourceString.groups();

		Tokens ret;
		for (auto g: groups) {
			auto sourceFiles = findFiles(g.concat());
			ret.insert(ret.end(), sourceFiles.begin(), sourceFiles.end());
		}

		return ret;
	}

	BuildTarget *getParent() ;

	void print() {
		vout << "target " << name << ": " << endl;
		for (auto &m: members) {
			vout << "\t" << m.first << " = " << m.second << " " << endl;
		}
		vout << endl;
	}

	string getCpp() {
		return get("cpp").concat();
	}


	time_t build() override {
		dirty = false;
		auto exe = targetPath();
		if (exe.empty() || name == "root") {
			return 0;
		}

		vout << endl;
		vout << "  target " << name << "..." << endl;

		auto lastDependency = 0;
		for (auto &d: dependencies) {
			auto t = d->build();
			if (t > lastDependency) {
				lastDependency = t;
			}
			if (t == 0) {
				dirty = true;
				break;
			}
		}

		if (lastDependency > getTimeChanged()) {
			dirty = true;
		}
		else if (!dirty) {
			cout << "nothing needs to be done for " << name << endl;
		}

		if (dirty) {

			string fileList;

			for (auto f: dependencies) {
				if (f->includeInBinary()) {
					fileList += (f->targetPath() + " ");
				}
			}

			string command = getCpp() + " -o " + exe + " " + fileList + " " + get("libs").concat() + " " + get("flags").concat();
			cout << command << endl;
			if (system (command.c_str())) {
				throw runtime_error("linking failed with\n" + command);
			}
			dirty = false;
			cout << endl;
			return time(0);
		}

		return getTimeChanged();
	}

	void clean() override {
		for (auto &d: dependencies) {
			d->clean();
		}
		vout << "removing file " << targetPath() << endl;
		remove(targetPath().c_str());
	}


	string targetPath() override {
		auto dir = get("dir").concat();
		if (!dir.empty()) {
			dir += "/";
		}
		return dir + get("exe").concat();
	}
};

class CopyFile: public Dependency {
public:
	CopyFile(const CopyFile &) = default;
	CopyFile(CopyFile &&) = default;
	CopyFile(string source, string output, BuildTarget *parent):
		source(source),
		output(output),
		parent(parent) {}

	string source;
	string output;
	BuildTarget *parent;

	time_t getSourceChangedTime() {
		return ::getTimeChanged(source);
	}

	time_t build() override {
		if (output == source) {
			vout << "file " << output << " source and target is on same place. skipping" << endl;
		}
		auto timeChanged = getTimeChanged();

		if (getSourceChangedTime() > timeChanged) {
			ifstream src(source);
			if (!src.is_open()) {
				cout << "could not open file " << source << " for copy for target " << parent->name << endl;
			}

			ofstream dst(output);
			if (!dst) {
				cout << "could not open file " << output << " for copy for target " << parent->name << endl;
			}

			vout << "copy " << source << " --> " << output << endl;
			dst << src.rdbuf();
		}
		return timeChanged;
	}

	void clean() override {
		vout << "removing file " << output << endl;
		remove(output.c_str());
	}

	string targetPath() override {
		return output;
	}

	bool includeInBinary() override {
		return false;
	}
};

class BuildFile: public Dependency {
public:
	BuildFile(const BuildFile &) = default;
	BuildFile(BuildFile &&) = default;
	BuildFile(string filename, string output, BuildTarget *parent):
		filename(filename),
		output(fixObjectEnding(output)),
		depFile(fixDepEnding(output)),
		parent(parent) {

	}
	string filename; //The source of the file
	string output; //The target of the file
	string depFile; //File that summarizes dependencies for file

	BuildTarget *parent;
	vector<string> dependencies;

	string fixObjectEnding(string filename) {
		if (filename.find(".cpp") == filename.size() - 4) {
			filename = string(filename.begin(), filename.end() - 4) + ".o";
		}
		return filename;
	}
	string fixDepEnding(string filename) {
		if (filename.find(".cpp") == filename.size() - 4) {
			return string(filename.begin(), filename.end() - 4) + ".d";
		}
		return {};
	}


	time_t getInputChangedTime() {
		return ::getTimeChanged(filename);
	}

	time_t getDepFileChangedTime() {
		return ::getTimeChanged(depFile);
	}

	vector<string> parseDepFile() {
		if (depFile.empty()) {
			return {};
		}
		ifstream file(depFile);
		if (file.is_open()) {
			vector <string> ret;
			string d;
			file >> d; //The first is the target path
			while (file >> d) {
				if (d != "\\") {
					ret.push_back(d);
				}
			}
			return ret;
		}
		else {
			cout << "could not find .d file for " << output << endl;
			return {};
		}
	}

	time_t build() override {
		auto flags = parent->get("flags").concat();
		auto depChangedTime = getDepFileChangedTime();
		auto inputChangedTime = getInputChangedTime();
		if (depChangedTime == 0 || inputChangedTime > depChangedTime) {
			string command = parent->getCpp() + " -MT " + output + " -MM " + filename + " " + flags + " > " + depFile;
			vout << command << endl;
			if (system(command.c_str())) {
				throw runtime_error("could not build dependencies with command\n\t" + command);
			}
		}

		auto timeChanged = getTimeChanged();

		auto dependencyFiles = parseDepFile();

		for (auto &d: dependencyFiles) {
			auto dependencyTimeChanged = ::getTimeChanged(d);
			if (dependencyTimeChanged == 0 || dependencyTimeChanged > timeChanged) {
				dirty = true;
			}
		}

		if (dirty) {
			string command = parent->getCpp() + " -c -o " + output + " " + filename + " " + flags;
			cout << command << endl;
			system (command.c_str());
			dirty = false;
			return time(0);
		}
		else {
			return getTimeChanged();
		}
	}

	string targetPath() override {
		return output;
	}

	void clean() override {
		vout << "removing file " << targetPath() << endl;
		remove(targetPath().c_str());
	}
};


class Environment {
public:
	vector<unique_ptr<BuildTarget>> targets;
	vector<unique_ptr<Dependency>> files;
	set<string> directories;

	Environment () {
		targets.emplace_back(new BuildTarget("root", this));
	}

	BuildTarget *findTarget(Token name) {
		if (name.empty()) {
			return nullptr;
		}
		for (auto &v: targets) {
			if (v->name == name) {
				return v.get();
			}
		}
		return nullptr;
	}

	BuildTarget *findVariable(Token name) {
		if (name.empty()) {
			return nullptr;
		}
		for (auto &v: targets) {
			if (v->name == name) {
				return v.get();
			}
		}
		return nullptr;
	}

	Tokens getValue(NameDescriptor name) {
		if (auto variable = findVariable(name.rootName)) {
			return variable->get(name.memberName);
		}
		else {
			return {};
		}
	}

	BuildTarget &operator[] (Token name) {
		if (auto v = findVariable(name)) {
			return *v;
		}
		else {
			targets.emplace_back(new BuildTarget(name, this));
			return *targets.back();
		}
	}

	void appendVariable(NameDescriptor name, Tokens value) {
		(*this) [name.rootName].append(name.memberName, value);
	}

	void setVariable(NameDescriptor name, Tokens value) {
		(*this) [name.rootName].set(name.memberName, value) ;
	}

	void print() {
		for (auto &v: targets) {
			v->print();
		}
	}



	std::string getOutputPath(Token target) {
		auto value = getValue({target, "dir"});

		return value.concat();
	}

	void calculateDependencies() {
		files.clear();
		for (auto &target: targets) {
			auto outputPath = getOutputPath(target->name);
			if (!outputPath.empty()) {
				outputPath += "/";
			}
			for (auto &file: target->getGroups("src")) {
				files.emplace_back(new BuildFile(file, outputPath + file, target.get()));
				target->addDependency(files.back().get());
			}
			for (auto &file: target->getGroups("copy")) {
				files.emplace_back(new CopyFile(file, outputPath + "/" + file, target.get()));
				target->addDependency(files.back().get());
			}
		}
	}

	void compile(vector<string> targetArguments) {
		print();

		calculateDependencies();

		for (auto &file: files) {
			directories.emplace(getDirectory(file->targetPath()));
		}

		for (auto dir: directories) {
			if (verbose) cout << "output dir: " << dir << endl;
			system(("mkdir -p " + dir).c_str());
		}


		if (targetArguments.empty()) {
			for (auto &target: targets) {
				target->build();
			}
		}
		else {
			for (auto t: targetArguments) {
				auto target = findTarget(t);
				if (target) {
					target->build();
				}
				else {
					cout << "target '" << t << "' does not exist" << endl;
				}
			}
		}
	}

	void clean() {
		calculateDependencies();
		for (auto &t: targets) {
			t->clean();
		}
	}

	void list() {
		for (auto &t: targets) {
			if (t->name != "root") {
				cout << t->name << " ";
			}
		}
		cout << "clean";
		cout << endl;
	}
};



BuildTarget *BuildTarget::getParent() {
	auto inheritFrom = get("inherit").concat();
	return env->findTarget(inheritFrom);
}


bool isSpecialChar(char c) {
	const string special = "+=.-:*";
	return special.find(c) != string::npos;
}

bool isOperator(string op) {
	vector<string> opList = {
		"=",
		"+=",
		"-=",
	};
	return find(opList.begin(), opList.end(), op) != opList.end();
}


Tokens tokenize(string line) {
	istringstream ss(line);
	Tokens ret;
	ret.emplace_back();

	auto newWord = [&] () {
		if (!ret.back().empty()) {
			ret.emplace_back();
		}
	};

	while (ss) {
		auto c = ss.get();
		if (c == -1) {
			continue;
		}
		if (isspace(c)) {
			ret.back().trailingSpace.push_back(c);
			while (isspace(ss.peek())) {
				ret.back().trailingSpace.push_back(ss.get());
			}
			newWord();
			continue;
		}
		else if (isSpecialChar(c)) {
			newWord();
			ret.back().push_back(c);
			while (isSpecialChar(ss.peek())) {
				ret.back().push_back(ss.get());
				if (ret.back().back() == '=') {
					break;
				}
			}
			if (!isspace(ss.peek())) {
				newWord();
			}
			continue;
		}
		else if (c == '#') {
			string line;
			getline(ss, line); //Remove the rest of the line
			continue;
		}

		ret.back().push_back(c);
	}
	if (ret.back().empty()) {
		ret.pop_back();
	}
	return ret;
}

string concatTokens(const vector<Token>::iterator begin, const vector<Token>::iterator end) {
	string ret;
	for (auto it = begin; it != end; ++it) {
		ret += ((*it) + it->trailingSpace);
	}
	return ret;
}


const char * helpText = R"_(
Matmake

arguments:
clean             remove all target files
[target]          build only specified target eg. debug or release
--local           do not build external dependencies (other folders)
-v or --verbose   print more information on what is happening
--list -l         print a list of available targets
)_";

int main(int argc, char **argv) {
	ifstream matmakefile("Matmakefile");
	if (!matmakefile.is_open()) {
		cout << "matmake: could not find Matmakefile in " << getCurrentWorkingDirectory() << endl;
		return -1;
	}

	string line;
	Environment environment;
	vector<string> targets;

	string operation = "build";
	bool localOnly;

	for (int i = 1; i < argc; ++i) {
		string arg = argv[i];
		if (arg == "clean") {
			operation = "clean";
		}
		else if(arg == "--local") {
			localOnly = true;
		}
		else if (arg == "-v" || arg == "--verbose") {
			verbose = true;
		}
		else if (arg == "--list" || arg == "-l") {
			operation = "list";
		}
		else if (arg == "all") {
			//Do nothing. Default is te build all
		}
		else {
			targets.push_back(arg);
		}
	}

	while (matmakefile) {
		getline(matmakefile, line);
		auto words = tokenize(line);

		if (!words.empty()) {
			auto it = words.begin() + 1;
			for (; it != words.end(); ++it) {
				if (isOperator(*it)) {
					break;
				}
			}
			if (it != words.end()) {
				auto argumentStart = it + 1;
				vector<Token> variableName(words.begin(), it);
				Tokens value(argumentStart, words.end());
				auto variableNameString = concatTokens(words.begin(), it);
				auto argumentValue = concatTokens(argumentStart, words.end());

				if (*it == "=") {
					environment.setVariable(variableName, value);
				}
				else if (*it == "+=") {
					environment.appendVariable(variableName, value);
				}
			}
			else if (!localOnly && words.size() > 2 && words.front() == "external") {
				cout << "external dependency to " << words[1] << endl;

				auto currentDirectory = getCurrentWorkingDirectory();

				if (!chdir(words[1].c_str())) {
					main(argc, argv);
				}
				else {
					cerr << "could not open directory " << words[1] << endl;
				}

				if (chdir(currentDirectory.c_str())) {
					throw runtime_error("could not go back to original working directory " + currentDirectory);
				}
				else {
					vout << "returning to " << currentDirectory << endl;
					cout << endl;
				}
			}
		}
	}

	if (operation == "build") {
		environment.compile(targets);
	}
	else if (operation == "list") {
		environment.list();
	}
	else if (operation == "clean") {
		environment.clean();
	}


	cout << "done..." << endl;
}



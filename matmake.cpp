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
#include <thread>
#include <mutex>
#include <queue>



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
bool debugOutput = false;
int numberOfThreads = thread::hardware_concurrency(); //Get the maximal number of threads
bool bailout = false;

//verbose output
#define vout if(verbose) cout
#define dout if(debugOutput) cout

//Creates a directory if it does not exist
void createDirectory(string dir) {
	if (system(("mkdir -p " + dir).c_str())) {
		runtime_error("could not create directory " + dir);
	}
}

#endif

time_t getTimeChanged(const string &path) {
	struct stat file_stat;
	int err = stat(path.c_str(), &file_stat);
    if (err != 0) {
    	dout << "notice: file does not exist: " << path << endl;
    	return 0;
    }
    return file_stat.st_mtime;
}

bool isDirectory(const string &path) {
	struct stat file_stat;
	int err = stat(path.c_str(), &file_stat);
    if (err != 0) {
    	dout << "file or directory " << path << " does not exist" << endl;
    	return false;
    }
    return file_stat.st_mode & S_IFDIR;
}


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
	else {
		return "";
	}
}

vector <string> listRecursive(string directory) {

	auto list = listFiles(directory);
	auto ret = list;

	for (auto &f: list) {
		if (isDirectory(directory + "/" + f)) {
			auto subList = listRecursive(directory + "/" + f);
			for (auto &s: subList) {
				s = f + "/" + s;
			}
			ret.insert(ret.end(), subList.begin(), subList.end());
		}
	}

	return ret;
}


struct Token: public string {
	string trailingSpace;
	struct Location {
		Location() = default;
		Location (long line, long col): line(line), col(col) {}
		long line = 0;
		long col = 0;
	} location;

	Token(const Token &t) = default;
	Token() = default;
	Token(Token && t) = default;
	Token &operator = (const Token &) = default;
	Token(const string str, Location l = {0, 0}): string(str), location(l) {}
	Token(const char* c, Location l = {0,0 }): string(c), location(l) {}
	Token(string::iterator begin, string::iterator end, Location l = {0,0 }): string(begin, end), location(l) {}

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

	string getLocationDescription() {
		stringstream ss;
		ss << "Matmakefile:" << location.line << ":" << location.col;
		return ss.str();
	}


};


vector<Token> findFiles(Token pattern) {
	pattern = Token(trim(pattern), pattern.location);
	auto found = pattern.find('*');

	vector<Token> ret;
	if (found != string::npos) {
		string beginning(pattern.begin(), pattern.begin() + found);
		string ending;
		string directory = beginning;
		string fileNameBeginning;
		auto directoryEnding = directory.rfind('/');
		if (directoryEnding != string::npos && directoryEnding < found) {
			fileNameBeginning = string(directory.begin() + directoryEnding + 1, directory.begin() + found);
			directory = string(directory.begin(), directory.begin() + directoryEnding);
		}

		vector<string> fileList;
		if (found + 1 < pattern.size() && pattern[found + 1] == '*') {
			// ** means recursive wildcard search
			fileList = listRecursive(directory);
			ending = string(pattern.begin() + found + 2, pattern.end());
		}
		else {
			// * means wildcard search
			fileList = listFiles(directory);
			ending = string(pattern.begin() + found + 1, pattern.end());
		}
		for (auto &file: fileList) {
			auto endingPos = file.find(ending);
			if (endingPos == string::npos) {
				if (ending.empty()) {
					if (file.find(fileNameBeginning) == 0) {
						ret.emplace_back(directory + "/" + file, pattern.location);
					}
				}
			}
			else {
				if (endingPos == file.size() - ending.size() &&
						file.find(fileNameBeginning) == 0) {
					ret.emplace_back(directory + "/" + file, pattern.location);
				}
			}
		}
	}
	else {
		return {pattern};
	}
	if (ret.empty()) {
		vout << "warning: pattern " << pattern << " does not match any file" << endl;
	}
	if (debugOutput && !ret.empty()) {
		dout << "recursively added:" << endl;
		for (auto &f: ret) {
			dout << f << " " << endl;
		}
	}
	return ret;
}

class MatmakeError: public runtime_error {
public:
	MatmakeError(Token t, string str): runtime_error(t.getLocationDescription() + ": error " + str  + "\n") {
		token = t;
	}

    Token token;
};


static std::pair<Token, Token> stripFileEnding(Token filename, bool allowNoMatch = false) {
	filename = trim(filename);
	if (filename.find(".cpp") == filename.size() - 4) {
		filename = Token(filename.begin(), filename.end() - 4, filename.location);
		return {filename, "cpp"};
	}
	else if (filename.find(".c") == filename.size() - 2) {
		filename = Token(filename.begin(), filename.end() - 2, filename.location);
		return {filename, "c"};
	}
	else if (filename.find(".so") == filename.size() - 3) {
		filename = Token(filename.begin(), filename.end() - 3, filename.location);
		return {filename, "so"};
	}
	else {
		if (allowNoMatch) {
			return {filename, Token("", filename.location)};
		}
		else {
			throw MatmakeError(filename, "unknown filetype in file " + filename);
		}
	}
}


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

	Token concat() {
		Token ret;
		for (auto it = begin(); it != end(); ++it) {
			ret += ((*it) + it->trailingSpace);
		}
		if (!empty()) {
			ret.location = front().location;
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
	class Environment *env;
	set<class Dependency*> dependencies;
	bool dirty = false;

	Dependency(Environment *env): env(env) {};

	virtual ~Dependency() {}

	time_t getTimeChanged() {
		return ::getTimeChanged(targetPath());
	}

	virtual time_t build() = 0;
	virtual void work() = 0;

	void addDependency(Dependency *file) {
		dependencies.insert(file);
	}

	//Add task to list of tasks to be executed
	void queue();

	virtual Token targetPath() = 0;

	virtual void clean() = 0;

	virtual bool includeInBinary() { return true; };

	virtual void addSubscriber(Dependency *s) {
		if (find(subscribers.begin(), subscribers.end(), s) == subscribers.end()) {
			subscribers.push_back(s);
		}
	}

	//Send a notice to all subscribers
	virtual void sendSubscribersNotice() {
		for (auto s: subscribers) {
			s->notice(this);
		}
		lock_guard<mutex> guard(accessMutex);
		subscribers.clear();
	}

	// A message from a object being subscribed to
	// This is used by targets to know when all dependencies
	// is built
	virtual void notice(Dependency *d) {};

	vector <Dependency*> subscribers;
	mutex accessMutex;
};


struct BuildTarget: public Dependency {
	Token name;
	map<Token, Tokens> members;
	set<Dependency *> waitList;
	Token command;

	BuildTarget(Token name, class Environment *env): Dependency(env), name(name) {
		if (name != "root") {
			assign("inherit", Token("root"));
		}
		else {
//			assign("exe", Token("program"));
			assign("cpp", Token("c++"));
			assign("cc", Token("cc"));
		}
	};

	BuildTarget(class Environment *env): Dependency(env){
		assign("inherit", Token("root"));
	};

	BuildTarget(NameDescriptor n, Tokens v, Environment *env): BuildTarget(n.rootName, env) {
		assign(n.memberName, v);
	}

	void inherit(const BuildTarget &parent) {
		for (auto v: parent.members) {
			if (v.first == "inherit") {
				continue;
			}
			assign(v.first, v.second);
		}
	}

	void assign(Token memberName, Tokens value) {
		members[memberName] = value;

		if (memberName == "inherit") {
			auto parent = getParent();
			if (parent) {
				inherit(*parent);
			}
		}
		else if (memberName == "exe" || memberName == "dll") {
			if (trim(value.concat()) == "%") {
				members[memberName] = name;
			}
		}

		if (memberName == "dll") {
			auto n = members[memberName];
			if (!n.empty()) {
				auto ending = stripFileEnding(n.concat(), true);
				if (!ending.second.empty()) {
					members[memberName] = Token(ending.first + ".so");
				}
				else {
					members[memberName] = Token(n.concat() + ".so");
				}
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

		if (ret.size() == 1) {
			if (ret.front().empty()) {
				vout << " no pattern matching for " << memberName << endl;
			}
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

	Token getCpp(Token filetype) {
		if (filetype == "cpp") {
			return get("cpp").concat();
		}
		else if (filetype == "c") {
			return get("cc").concat();
		}
		else {
			return "echo";
		}
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
			if (d->dirty) {
				d->addSubscriber(this);
				accessMutex.lock();
				waitList.insert(d);
				accessMutex.unlock();
			}
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
			cout << name << " is fresh " << endl;
		}

		if (dirty) {
			Token fileList;

			for (auto f: dependencies) {
				if (f->includeInBinary()) {
					fileList += (f->targetPath() + " ");
				}
			}

			auto cpp = getCpp("cpp");
			if (!get("dll").empty()) {
				command = cpp + " -shared -o " + exe + " -Wl,--start-group " + fileList + " " + get("libs").concat() + "  -Wl,--end-group  " + get("flags").concat();
			}
			else {
				command = cpp + " -o " + exe + " -Wl,--start-group " + fileList + " " + get("libs").concat() + "  -Wl,--end-group  " + get("flags").concat();
			}
			command.location = cpp.location;

			if (waitList.empty()) {
				queue();
			}

			return time(0);
		}

		return getTimeChanged();
	}

	void notice(Dependency * d) override {
		accessMutex.lock();
		waitList.erase(waitList.find(d));
		accessMutex.unlock();
		vout << d->targetPath() << " removed from wating list from " << name << " " << waitList.size() << " to go" << endl;
		if (waitList.empty()) {
			queue();
		}
	}

	// This is called when all dependencies are built
	void work() override {
		vout << "linking " << name << endl;
		vout << command << endl;
		if (system (command.c_str())) {
			throw MatmakeError(command, "linking failed with\n" + command);
		}
		dirty = false;
		sendSubscribersNotice();
		vout << endl;
	}

	void clean() override {
		for (auto &d: dependencies) {
			d->clean();
		}
		vout << "removing file " << targetPath() << endl;
		remove(targetPath().c_str());
	}


	Token targetPath() override {
		auto dir = get("dir").concat();
		if (!dir.empty()) {
			dir += "/";
		}
		auto exe = get("exe");
		if (exe.empty()) {
			return dir + get("dll").concat();
		}
		else {
			return dir + get("exe").concat();
		}
	}

	Token getOutputDir() {
		auto outputPath = get("dir").concat();
		if (!outputPath.empty()) {
			outputPath += "/";
		}
		return outputPath;
	}

	//If set, where the obj-files is placed
	Token getObjDir() {
		auto outputPath = get("objdir").concat();
		if (!outputPath.empty()) {
			outputPath += "/";
		}
		else {
			return getOutputDir();
		}
		return outputPath;
	}
};


class CopyFile: public Dependency {
public:
	CopyFile(const CopyFile &) = default;
	CopyFile(CopyFile &&) = default;
	CopyFile(Token source, Token output, BuildTarget *parent, Environment *env):
		Dependency(env),
		source(source),
		output(output),
		parent(parent) {}

	Token source;
	Token output;
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
			queue();
			dirty = true;

			return time(0);
		}
		return timeChanged;
	}

	void work() override {
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

		sendSubscribersNotice();
	}

	void clean() override {
		vout << "removing file " << output << endl;
		remove(output.c_str());
	}

	Token targetPath() override {
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
	BuildFile(Token filename, BuildTarget *parent, class Environment *env):
		Dependency(env),
		filename(filename),
		output(fixObjectEnding(parent->getObjDir() + filename)),
		depFile(fixDepEnding(parent->getObjDir() + filename)),
		filetype(stripFileEnding(filename).second),
		parent(parent) {
		auto withoutEnding = stripFileEnding(parent->getObjDir() + filename);
		if (withoutEnding.first.empty()) {
			throw MatmakeError(filename, "could not figure out source file type '" + parent->getObjDir() + filename +
					"' . Is the file ending right?");
		}
		if (filename.empty()) {
			throw MatmakeError(filename, "empty buildfile added");
		}
		if (output.empty()) {
			throw MatmakeError(filename, "could not find target name");
		}
		if (depFile.empty()) {
			throw MatmakeError(filename, "could not find dep filename");
		}
	}
	Token filename; //The source of the file
	Token output; //The target of the file
	Token depFile; //File that summarizes dependencies for file
	Token filetype; //The ending of the filename

	Token command;
	Token depCommand;

	BuildTarget *parent;
	vector<string> dependencies;


	static Token fixObjectEnding(Token filename) {
		return stripFileEnding(filename).first + ".o";
	}
	static Token fixDepEnding(Token filename) {
		return stripFileEnding(filename).first + ".d";
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
			dout << "could not find .d file for " << output << " --> " << depFile << endl;
			return {};
		}
	}

	time_t build() override {
		auto flags = parent->get("flags").concat();
		if (filetype == "cpp") {
			auto cppflags = parent->get("cppflags");
			if (!cppflags.empty()) {
				flags += (" " + cppflags.concat());
			}
		}
		if (filetype == "c") {
			auto cflags = parent->get("cflags");
			if (!cflags.empty()) {
				flags += (" " + cflags.concat());
			}
		}
		bool shouldQueue = false;

		auto depChangedTime = getDepFileChangedTime();
		auto inputChangedTime = getInputChangedTime();

		auto dependencyFiles = parseDepFile();

		if (depChangedTime == 0 || inputChangedTime > depChangedTime || dependencyFiles.empty()) {
			depCommand = parent->getCpp(filetype) + " -MT " + output + " -MM " + filename + " " + flags + " > " + depFile;
			shouldQueue = true;
		}

		auto timeChanged = getTimeChanged();


		for (auto &d: dependencyFiles) {
			auto dependencyTimeChanged = ::getTimeChanged(d);
			if (dependencyTimeChanged == 0 || dependencyTimeChanged > timeChanged) {
				dirty = true;
				break;
			}
		}

		if (shouldQueue || dirty) {
			dirty = true;
			queue();
		}

		if (dirty) {
			command = parent->getCpp(filetype) + " -c -o " + output + " " + filename + " " + flags;
			command.location = filename.location;

			return time(0);
		}
		else {
			return getTimeChanged();
		}
	}

	void work() override {
		if (!depCommand.empty()) {
			vout << depCommand << endl;
			if (system(depCommand.c_str())) {
				throw MatmakeError(command, "could not build dependencies with command\n\t" + command);
			}
		}

		if (!command.empty()) {
			vout << command << endl;
			if (system (command.c_str())) {
				throw MatmakeError(command, "could not build object with command\n\t" + command);
			}
			dirty = false;
			sendSubscribersNotice();
		}

	}

	Token targetPath() override {
		return output;
	}

	void clean() override {
		vout << "removing file " << targetPath() << endl;
		remove(targetPath().c_str());
		if (!depFile.empty()) {
			vout << "removing file " << depFile << endl;
			remove(depFile.c_str());
		}
	}
};


class Environment {
public:
	vector<unique_ptr<BuildTarget>> targets;
	vector<unique_ptr<Dependency>> files;
	set<string> directories;
	queue<Dependency *> tasks;
	mutex workMutex;
	mutex workAssignMutex;
	int maxTasks = 0;
	int lastProgress = 0;
	bool finished = false;

	void addTask(Dependency *t) {
		workAssignMutex.lock();
		tasks.push(t);
		maxTasks = std::max((int)tasks.size(), maxTasks);
		workAssignMutex.unlock();
		workMutex.unlock();
	}

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
		(*this) [name.rootName].assign(name.memberName, value) ;
	}

	void print() {
		for (auto &v: targets) {
			v->print();
		}
	}

	void printProgress() {
		if (!maxTasks) {
			return;
		}
		int amount = (100 - tasks.size() * 100 / maxTasks);

		if (amount == lastProgress) {
			return;
		}
		lastProgress = amount;

		stringstream ss;

		if (!debugOutput && !verbose) {
			ss << "[";
			for (int i = 0; i < amount / 4; ++i) {
				ss << "-";
			}
			if (amount < 100) {
				ss << ">";
			}
			else {
				ss << "-";
			}
			for (int i = amount / 4; i < 100 / 4; ++i) {
				ss << " ";
			}
			ss << "] " << amount << "%  \r";
		}
		else {
			ss << "[" << amount << "%] ";
		}

		cout << ss.str();
		cout.flush();
	}


	void calculateDependencies() {
		files.clear();
		for (auto &target: targets) {
			auto outputPath = target->getOutputDir();

			target->print();
			dout << "target " << target->name << " src " << target->get("src").concat() << endl;
			for (auto &filename: target->getGroups("src")) {
				if (filename.empty()) {
					continue;
				}
				files.emplace_back(new BuildFile(filename, target.get(), this));
				target->addDependency(files.back().get());
			}
			for (auto &file: target->getGroups("copy")) {
				if (file.empty()) {
					continue;
				}
				files.emplace_back(new CopyFile(file, outputPath + "/" + file, target.get(), this));
				target->addDependency(files.back().get());
			}
		}
	}

	void compile(vector<string> targetArguments) {
		print();

		calculateDependencies();

		for (auto &file: files) {
			auto dir = getDirectory(file->targetPath());
			if (dir.empty()) {
				continue;
			}
			directories.emplace(dir);
		}

		for (auto dir: directories) {
			dout << "output dir: " << dir << endl;
			if (dir.empty()) {
				continue;
			}
			if (isDirectory(dir)) {
				continue; //Skip if already created
			}
			createDirectory(dir);
		}


		if (targetArguments.empty()) {
			for (auto &target: targets) {
				target->build();
			}
		}
		else {
			bool matchFailed = true;
			for (auto t: targetArguments) {
				auto target = findTarget(t);
				if (target) {
					target->build();
					matchFailed = false;
				}
				else {
					cout << "target '" << t << "' does not exist" << endl;
				}

				if (matchFailed) {
					cout << "run matmake --help for help" << endl;
					cout << "targets: ";
					list();
					bailout = true;
				}
			}
		}

		work();
	}

	void work() {
		vector<thread> threads;
		vector<bool> status;


		if (numberOfThreads < 2) {
			numberOfThreads = 1;
			vout << "running with 1 thread" << endl;
		}
		else {
			vout << "running with " << numberOfThreads << " threads" << endl;
		}

		if (numberOfThreads > 1) {
			threads.reserve(numberOfThreads);
			for (int i = 0 ;i < numberOfThreads; ++i) {
				status.push_back(true);
				threads.emplace_back([&, i] () {
					dout << "starting thread " << endl;

					while (!finished) {
						workAssignMutex.lock();
						status[i] = true;
						if (tasks.empty()) {
							status[i] = false;

							auto stillWorking = false;
							for (auto s: status) {
								if (s) {
									stillWorking = true;
									break;
								}
							}
							workAssignMutex.unlock();
							if (!stillWorking) {
								finished = true;
								dout << "thread " << i << ": no more work to do - stopping" << endl;
								workMutex.unlock();
							}
							else {
								dout << "thread " << i << " is waiting" << endl;
								workMutex.lock(); //Wait for next work
								dout << "thread " << i << " is running again" << endl;
							}
						}
						else {
							auto t = tasks.front();
							tasks.pop();
							workAssignMutex.unlock();
							try {
								t->work();
							}
							catch (MatmakeError &e) {
								cerr << e.what() << endl;
								finished = true;
								bailout = true;
							}
							printProgress();
						}
					}

					dout << "thread " << i << " is finished" << endl;
					if (finished) {
						workMutex.unlock();
					}
				});
			}
		}
		else {
			while(!tasks.empty()) {
				auto t = tasks.front();
				tasks.pop();
				try {
					t->work();
				}
				catch (MatmakeError &e) {
					dout << e.what() << endl;
					bailout = true;
				}
			}
		}

		for (auto &t: threads) {
			t.join();
		}
	}

	void clean(vector<string> targetArguments) {
		calculateDependencies();

		if (targetArguments.empty()) {
			for (auto &target: targets) {
				target->clean();
			}
		}
		else {
			for (auto t: targetArguments) {
				auto target = findTarget(t);
				if (target) {
					target->clean();
				}
				else {
					cout << "target '" << t << "' does not exist" << endl;
				}
			}
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


void Dependency::queue() {
	env->addTask(this);
}

bool isOperator(string &op) {
	vector<string> opList = {
		"=",
		"+=",
		"-=",
	};
	return find(opList.begin(), opList.end(), op) != opList.end();
}


Tokens tokenize(string line, int lineNumber) {
	istringstream ss(line);
	Tokens ret;

	// The column index for the current character
	int col = 1;

	auto get = [&] () {
		++col;
		return ss.get();
	};

	auto newWord = [&] () {
		if (ret.empty() || !ret.back().empty()) {
			ret.emplace_back();
			ret.back().location.line = lineNumber;
			ret.back().location.col = col;
		}
	};

	newWord();

	while (ss) {
		auto c = get();
		if (c == -1) {
			continue;
		}
		if (isspace(c)) {
			ret.back().trailingSpace.push_back(c);
			while (isspace(ss.peek())) {
				ret.back().trailingSpace.push_back(get());
			}
			newWord();
			continue;
		}
		else if (isSpecialChar(c)) {
			newWord();
			ret.back().push_back(c);
			while (isSpecialChar(ss.peek())) {
				ret.back().push_back(get());
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


Token concatTokens(const vector<Token>::iterator begin, const vector<Token>::iterator end) {
	Token ret;
	for (auto it = begin; it != end; ++it) {
		ret += ((*it) + it->trailingSpace);
	}
	if (begin != end) {
		ret.location = begin->location;
	}
	return ret;
}



const char * helpText = R"_(
Matmake

A fast simple build system. It can be installed on the system or included with the source code

arguments:
[target]          build only specified target eg. debug or release
clean             remove all target files
clean [target]    remove specified target files
--local           do not build external dependencies (other folders)
-v or --verbose   print more information on what is happening
-d or --debug     print debug messages
--list -l         print a list of available targets
-j [n]            use [n] number of threads
-j 1              run in single thread mode
--help or -h      print this text
--init            create a cpp project in current directory
--init [dir]      create a cpp project in the specified directory
--example         show a example of a Matmakefile


Matmake defaults to use optimal number of threads for the computer to match the number of cores.
)_";


const char * exampleMain = R"_(
#include <iostream>

using namespace std;

int main(int argc, char **argv) {
	cout << "Hello" << endl;
	
	return 0;
}

)_";

const char *exampleMatmakefile = R"_(
# Matmake file
# https://github.com/mls-m5/matmake

cppflags += -std=c++11      # c++ only flags
cflags +=                  # c only flags 
flags += -Iinclude         # global flags

## Main target
main.flags += -W -Wall -Wno-unused-parameter -Wno-sign-compare #-Werror
main.src = src/*.cpp
main.exe = program         # name of executable
main.libs += # -lGL -lSDL  # libraries to add at link time
# main.dir = bin/release   # set build path
# main.objdir = bin/obj    # separates obj-files from build files
# main.dll = lib           # use this instead of exe to create so/dll file

## Debug target:
# main_debug.inherit = main  # copy all settings from main
# main_debug.flags += -g -O0
# main_debug.dir = bin/debug
# main_debug.objdir = bin/debugobj

)_";


int createProject(string dir) {
	if (!dir.empty()) {
		createDirectory(dir);

		if (chdir(dir.c_str())) {
			cerr << "could not create directory " << dir << endl;
			return -1;
		}
	}

	if (getTimeChanged("Matmakefile") > 0) {
		cerr << "Matmake file already exists. Exiting..." << endl;
		return -1;
	}

	{
		ofstream file("Matmakefile");
		file << exampleMatmakefile;
	}

	createDirectory("src");

	if (getTimeChanged("src/main.cpp") > 0) {
		cerr << "src/main.cpp file already exists. Exiting..." << endl;
		return -1;
	}

	createDirectory("include");

	{
		ofstream file("src/main.cpp");
		file << exampleMain;
	}

	cout << "project created";
	if (!dir.empty()) {
		cout << " in " << dir;
	}
	cout << "..." << endl;
	return -1;
}

int main(int argc, char **argv) {
	auto startTime = time(0);
	ifstream matmakefile("Matmakefile");

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
		else if (arg == "--help" || arg == "-h") {
			cout << helpText << endl;
			return 0;
		}
		else if (arg == "--example") {
			cout << "### Example matmake file (Matmakefile): " << endl;
			cout << "# means comment" << endl;
			cout << exampleMatmakefile << endl;
			cout << "### Example main file (src/main.cpp):" << endl;
			cout << exampleMain << endl;
			return 0;
		}
		else if (arg == "--debug" || arg == "-d") {
			debugOutput = true;
			verbose = true;
		}
		else if (arg == "-j") {
			++i;
			if (i < argc) {
				numberOfThreads = atoi(argv[i]);
			}
			else {
				cerr << "expected number of threads after -j argument" << endl;
				return -1;
			}
		}
		else if (arg == "--init") {
			++i;
			if (i < argc) {
				string arg = argv[i];
				if (arg[0] != '-') {
					return createProject(argv[i]);
				}
			}
			return createProject("");
		}
		else if (arg == "all") {
			//Do nothing. Default is te build all
		}
		else {
			targets.push_back(arg);
		}
	}

	if (!matmakefile.is_open()) {
		if (getTimeChanged("Makefile") || getTimeChanged("makefile")) {
			cout << "makefile in " << getCurrentWorkingDirectory() << endl;
			string arguments = "make";
			for (int i = 1; i < argc; ++i) {
				arguments += (string(" ") + argv[i]);
			}
			arguments += " -j";
			system(arguments.c_str());
			cout << endl;
		}
		else {
			cout << "matmake: could not find Matmakefile in " << getCurrentWorkingDirectory() << endl;
		}
		return -1;
	}

	int lineNumber = 1;

	while (matmakefile) {
		getline(matmakefile, line);
		auto words = tokenize(line, lineNumber);

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
			else if (!localOnly && words.size() >= 2 && words.front() == "external") {
				cout << "external dependency to " << words[1] << endl;

				auto currentDirectory = getCurrentWorkingDirectory();

				if (!chdir(words[1].c_str())) {
					main(argc, argv);
					if (bailout) {
						break;
					}
					cout << endl;
				}
				else {
					cerr << "could not open directory " << words[1] << endl;
				}

				if (chdir(currentDirectory.c_str())) {
					throw runtime_error("could not go back to original working directory " + currentDirectory);
				}
				else {
					vout << "returning to " << currentDirectory << endl;
					vout << endl;
				}
			}
		}
		++lineNumber;
	}

	if (!bailout) {
		try {
			if (operation == "build") {
				environment.compile(targets);
			}
			else if (operation == "list") {
				environment.list();
				return 0;
			}
			else if (operation == "clean") {
				environment.clean(targets);
			}
		}
		catch (MatmakeError &e) {
			cerr << e.what() << endl;
		}
	}
	auto endTime = time(0);

	auto duration = endTime - startTime;
	auto m = duration / 60;
	auto s = duration - m * 60;

	cout << endl;
	if (bailout) {
		cout << "failed... " << m << "m " << s << "s" << endl;
		return -1;
	}
	else {
		string doneMessage = "done...";
		if (operation == "clean") {
			doneMessage = "cleaned...";
		}
		cout << doneMessage << " " << m << "m " << s << "s" << endl;
		return 0;
	}
}



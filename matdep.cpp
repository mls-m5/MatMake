
//MatDep: A wrapper around the g++ -MM command
//Part of he MatMake build system

#include <iostream>
#include <fstream>
#include <string>
#include <list>

using namespace std;

string changeToOEnding(string text) {
	auto f = text.find(".");
	if (f == string::npos) {
		return text;
	}
	return text.substr(0, f) + ".o";
}

int main(int argc, char **argv) {
	cout << "# MatDep 0.0" << endl;
	
	list<string> files;
	list<string> flags;

	for (int i = 1; i < argc; ++i) {
		string arg = argv[i];
		if (arg.find(".cpp") != string::npos || arg.find(".o") != string::npos) {
			files.push_back(arg);
		}
		else {
			flags.push_back(arg);
		}
	}

	string flagstring;
	for (auto it: flags) {
		flagstring += (" " + it);
	}

	for (auto it: files) {
		auto runString = "g++ -MM " + it + " -MT " + changeToOEnding(it) + flagstring;
		cout << "# " << runString << endl;
		FILE *fp = popen(runString.c_str(), "r");
		char buf[8];

		string returnString;
		while (fgets(buf, 8, fp)) {
			returnString += buf;
		}

		cout << returnString << endl;

		fclose(fp);
	}
}

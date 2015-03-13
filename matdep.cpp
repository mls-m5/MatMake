
//MatDep: A wrapper around the g++ -MM that does not strip the file paths
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

string fixBegining(string text, string actualBeginning) {
	auto f = text.find(':');
	if (f == string::npos) {
		return text;
	}

	text = text.substr(f, text.size() - f);
	return changeToOEnding(actualBeginning) + " " + text; //stripping cpp ending
}

int main(int argc, char **argv) {
	cout << "# MatDep 0.0" << endl;
	
	list<string> files;
	list<string> flags;

	for (int i = 1; i < argc; ++i) {
		string arg = argv[i];
		if (arg.find(".cpp") != string::npos or arg.find(".o") != string::npos) {
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
		auto runString = "g++ -MM " + it + flagstring;
		cout << "# " << runString << endl;
		FILE *fp = popen(runString.c_str(), "r");
		char buf[8];

		string returnString;
		while (fgets(buf, 8, fp)) {
			returnString += buf;
		}

		cout << fixBegining(returnString, it) << endl;

		fclose(fp);
	}
}

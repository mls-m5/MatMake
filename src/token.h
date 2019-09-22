#pragma once

#include <string>
#include <sstream>
#include <vector>



bool isSpecialChar(char c) {
	using std::string;
	const string special = "+=.-:*";
	return special.find(c) != string::npos;
}


struct Token: public std::string {
	std::string trailingSpace;
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
	Token(const std::string str, Location l = {0, 0}): std::string(str), location(l) {}
	Token(const char* c, Location l = {0,0 }): std::string(c), location(l) {}
	Token(std::string::iterator begin, std::string::iterator end, Location l = {0,0 }): std::string(begin, end), location(l) {}

	friend std::ostream &operator << (std::ostream &s, const Token &t) {
		if (t.empty()) {
			s << "empty ";
		}
		s << std::string(t) << t.trailingSpace;
		return s;
	}

	std::string str() {
		return *this;
	}

	std::string getLocationDescription() {
		std::stringstream ss;
		ss << "Matmakefile:" << location.line << ":" << location.col;
		return ss.str();
	}


};



struct Tokens: public std::vector<Token> {
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



Tokens tokenize(std::string line, int lineNumber) {
	using std::string;
	std::istringstream ss(line);
	Tokens ret;

	// The column index for the current character
	int col = 1;

	auto get = [&] () {
		++col;
		return static_cast<char>(ss.get());
	};

	auto newWord = [&] () {
		if (ret.empty() || !ret.back().empty()) {
			ret.emplace_back();
			ret.back().location.line = lineNumber;
			ret.back().location.col = col;
		}
	};

	newWord();

	while (ss && isspace(ss.peek())) {
		ss.get(); //Clear spaces in beginning of line
	}

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
			while (isSpecialChar(static_cast<char>(ss.peek()))) {
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


Token concatTokens(const std::vector<Token>::iterator begin, const std::vector<Token>::iterator end) {
	Token ret;
	for (auto it = begin; it != end; ++it) {
		ret += ((*it) + it->trailingSpace);
	}
	if (begin != end) {
		ret.location = begin->location;
	}
	return ret;
}


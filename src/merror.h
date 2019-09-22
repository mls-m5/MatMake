#pragma once

#include <stdexcept>
#include <token.h>
#include <string>


class MatmakeError: public std::runtime_error {
public:
	MatmakeError(Token t, std::string str): runtime_error(t.getLocationDescription() + ": error: " + str  + "\n") {
		token = t;
	}

	Token token;
};


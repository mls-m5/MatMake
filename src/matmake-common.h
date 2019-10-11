// Copyright Mattias Larsson Sköld
#pragma once

#include "token.h"

//! Describes the full name of a variable,
//! ie target.propertyName
struct NameDescriptor {
	NameDescriptor(std::vector<Token> name) {
		if (name.size() == 1) {
			propertyName = name.front();
		}
		else if (name.size() == 3 && name[1] == ".") {
			rootName = name.front();
			propertyName = name.back();
		}
	}

	NameDescriptor(const Token &name, const Token &propertyName): rootName(name), propertyName(propertyName) {}

	bool empty() {
		return propertyName.empty();
	}

	Token rootName = "root";
	Token propertyName = "";
};


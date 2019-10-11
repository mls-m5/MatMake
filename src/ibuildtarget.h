#pragma once

#include <token.h>
#include <map>


class IBuildTarget {
public:
	virtual ~IBuildTarget() = default;

	virtual time_t build() = 0;

	//! Gets a list of const properties to iterate over
	virtual const std::map<Token, Tokens> &properties() const = 0;

	//! Get a single editable property
	virtual Tokens &property(Token) = 0;

	//! Get but without reference
	//! (This may very well be redundant
	virtual Tokens get(const Token &propertyName) = 0;

	//! Used for setting _and handling_ properties on a build target
	//! This handles for example "inherit" or "exe" rules in the right way
	virtual void assign(Token propertyName, Tokens value) = 0;

	//! Adding extra data to a variable (existing or non-existing)
	virtual void append(Token propertyName, Tokens value) = 0;

	virtual Token getBuildDirectory() = 0;

	virtual Token getCompiler(Token filetype) = 0;

	virtual void print() = 0;
	virtual void clean() = 0;
};

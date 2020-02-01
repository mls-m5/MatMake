/*
 * matmake.cpp
 *
 *  Created on: 9 apr. 2019
 *      Author: Mattias Lasersköld
 *
 * The main file to be compiled
 * Notice that the design pattern of this file is not to be used
 * for inspiration in any actual program. The single source file design
 * is simply chosen to be able to compile the whole build system with one
 * command line on any system.
 *
 * To build matmake run "make" in parent directory, or run eqvivalent to
 * `c++ matmake.cpp -std=c++14 -o ../matmake`
 */


#include "matmake.h"

Globals globals;

int main(int argc, char **argv) {
	vector<string> args(argv + 1, argv + argc);
	start(args);
}


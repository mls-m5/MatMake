#pragma once

#include <iostream>

//verbose output
#define vout if(verbose) std::cout
#define dout if(debugOutput) std::cout

extern bool verbose;
extern bool debugOutput;


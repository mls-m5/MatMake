#pragma once

#include <iostream>

//verbose output
#define vout if(env().globals().verbose) std::cout
#define dout if(env().globals().debugOutput) std::cout

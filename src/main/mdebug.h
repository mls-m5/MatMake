#pragma once

#include <iostream>

#include "environment/globals.h"

// verbose output
#define vout                                                                   \
    if (globals.verbose)                                                       \
    std::cout
#define dout                                                                   \
    if (globals.debugOutput)                                                   \
    std::cout

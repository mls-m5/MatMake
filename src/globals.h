// Copyright Mattias Larsson Sköld 2019

#pragma once

#include <thread>

//! Global variables shared by the whole program
//!
struct Globals {
	bool verbose = false;
	bool debugOutput = false;

	size_t numberOfThreads = std::thread::hardware_concurrency(); //Get the maximal number of threads
	bool bailout = false; // when true: exit the program in a controlled way
};

extern Globals globals;

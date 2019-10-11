
// Global variables used in program
// Separated to be make it possible to add to Unit tests
// Make sure to only include this file once in every program

#include <thread>


bool verbose = false;
bool debugOutput = false;

namespace {

auto numberOfThreads = thread::hardware_concurrency(); //Get the maximal number of threads
bool bailout = false; // when true: exit the program in a controlled way

}

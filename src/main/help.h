#pragma once

const char *helpText = R"_(
Matmake

A fast simple build system. It can be installed on the system or included with the source code

arguments:
[target]          build only specified target eg. debug or release
clean             remove all target files
clean [target]    remove specified target files
rebuild           first run clean and then build
test              run all tests
-c or --config    specify build type: eg release or debug
--local           do not build external dependencies (other folders)
-v or --verbose   print more information on what is happening
-d or --debug     print debug messages
--list -l         print a list of available targets
-j [n]            use [n] number of threads
-j 1              run in single thread mode
--help or -h      print this text
--init            create a cpp project in current directory
--init [dir]      create a cpp project in the specified directory
--example         show a example of a Matmakefile
config=debug      example: add debug to default config flag.


Matmake defaults to use optimal number of threads for the computer to match the number of cores.
)_";

const char *exampleMain = R"_(
#include <iostream>

int main(int argc, char **argv) {
    std::cout << "Hello\n";

    return 0;
}

)_";

const char *exampleMatmakefile =
    R"_(# Matmake file
# https://github.com/mls-m5/matmake

cppflags +=                 # c++ only flags
cflags +=                   # c only flags

# global flags:
config += c++14 Wall
debug: config += debug      # Flags only used in "debug" confinguration

## Main target
main.includes +=
    include

main.src =
    src/*.cpp
    # multi line values starts with whitespace
# main.libs += # -lGL -lSDL2 # libraries to add at link time

## Unit test
# some_test.src = test/some_test.cpp
# some_test.out = test some_test

# main.out = main          # name of executable (not required)
# main.out = shared main   # create a shared library (dll/so)
# main.out = %             # set the output file name to the same as the target name
# main.link = [libname]    # link to shared or static library with targetname libname
# main.dir = build/bin     # set build path
# main.objdir = build/obj  # separates obj-files from build files
# main.sysincludes +=      # include files that should not show errors
# main.define += X         # define macros in program like #define
# external tests           # call matmake or make in another folder after build
# dependency some-dir      # same as external but before build in the current dir

)_";

const char *exampleMakefile = R"_(
# build settings is in file 'Matmakefile'


all:
    @echo using Matmake buildsystem
    @echo for more options use 'matmake -h'
    matmake

clean:
    matmake clean

)_";

Matmake
======================
A simple dependency calculating system for makefiles

In the example below the variable CPP_FILES contains names to all source files that you want to include in the project

in the script above all the source files is located in a folder named "src"

Add the following to your makefile:

#automatically find files from wildcards
CPP_FILES+= $(wildcard src/*.cpp)
OBJECTS = $(CPP_FILES:.cpp=.o)  #find the associated o-filenames
CXXFLAGS = #flags like -std=c++11 or -Iinclude/
LIBS = #libs like -lGL
#TARGET = name_of_program  #uncomment this line

#make sure to call the rule
all: ${TARGET} .depend

${TARGET}: ${OBJECTS}
	${CXX} ${CXXFLAGS} -o ${TARGET} ${OBJECTS} ${LIBS}

#rule for calculate dependencies
.depend: ${CPP_FILES} Makefile
	git submodule update --init #pull all submodules from server
	make -C matmake/
	@echo
	@echo Calculating dependencies
	matmake/matdep ${CPP_FILES} ${CXXFLAGS} > .depend
	
#include the dependencies
include .depend

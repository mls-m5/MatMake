
CXX = c++
TESTSRC = $(wildcard src/*.cpp)
TESTOBJ = $(TESTSRC:.cpp=.o)
CXXFLAGS = -std=c++11 -W -Wall -Wno-unused-parameter -Wno-sign-compare

all: matdep matmake

debug: CXXFLAGS+= -g -O0 -D_GLIBCXX_DEBUG -pthread
debug: all
	@echo Debugging enabled

tests: all .depend $(TESTOBJ) 
	./matdep $(TESTSRC) > .depend #testing
	./matdep $(TESTSRC) ##For visible output

matdep: matdep.cpp

matmake: matmake.cpp

.depend: Makefile $(TESTSRC)

clean:
	rm -f matdep
	rm -f matmake
	rm -f $(TESTOBJ)
	
init-project:
	mkdir -p ../src
	mkdir -p ../include
	cp -i examples/Matmake-makefile.txt ../Makefile
	cp -i examples/Matmakefile.txt ../Matmakefile
	cp -i examples/main.cpp ../src/
	cp -i examples/common.h ../include/
	@echo successfully created project
	@echo now, configure your Matmakefile and run 'make' or 'matmake/matmake' to build project

init-matdep-project:
	mkdir -p ../src
	mkdir -p ../include
	cp -i examples/Makefile-example.txt ../Makefile
	cp -i examples/main.cpp ../src/
	cp -i examples/common.h ../include/
	cp -i examples/gitignore ../.gitignore
	@echo successfully created project
	@echo now, configure your makefile and run make to build project

init-cmake-project:
	mkdir -p ../src
	mkdir -p ../include
	mkdir -p ../build-debug
	mkdir -p ../build-release
	cp -i examples/main.cpp ../src/
	cp -i examples/common.h ../include/
	cp -i examples/gitignore ../.gitignore
	cp -i examples/CMakeLists-root.txt ../CMakeLists.txt
	(cd ../build-debug && cmake -DCMAKE_BUILD_TYPE=Debug ..)
	(cd ../build-release && cmake -DCMAKE_BUILD_TYPE=Release ..)
	
refresh-cmake-files:
	(cd ../build-debug && cmake -DCMAKE_BUILD_TYPE=Debug ..)
	(cd ../build-release && cmake -DCMAKE_BUILD_TYPE=Release ..)
	

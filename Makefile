
CXX = c++
TESTSRC = $(wildcard src/*.cpp)
TESTOBJ = $(TESTSRC:.cpp=.o)
CXXFLAGS = -std=c++14 -W -Wall

matmake = matmake

ifeq ($(OS),Windows_NT)
matmake = matmake.exe
endif

all: ${matmake}

debug: CXXFLAGS+= -g -O0 -D_GLIBCXX_DEBUG
debug: all
	@echo Debugging enabled

tests: all .depend $(TESTOBJ) 
	./matdep $(TESTSRC) > .depend #testing
	./matdep $(TESTSRC) ##For visible output

matdep: matdep.cpp

${matmake}: CXXFLAGS += -pthread
${matmake}: src/*.cpp src/*.h
	${CXX} -o ${matmake} src/matmake.cpp -Isrc/ ${CXXFLAGS}

.depend: Makefile $(TESTSRC)

clean:
	rm -f matdep
	rm -f ${matmake}
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

install: matmake
	sudo cp -i matmake  /usr/local/bin
	
remove: matmake
	sudo cp -i /usr/local/bin

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
	

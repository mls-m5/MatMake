
TESTSRC = $(wildcard src/*.cpp)
TESTOBJ = $(TESTSRC:.cpp=.o)
CXXFLAGS = -std=c++11

all: matdep

tests: all .depend $(TESTOBJ) 
	./matdep $(TESTSRC) > .depend #testing
	./matdep $(TESTSRC) ##For visible output

matdep: matdep.cpp

.depend: Makefile $(TESTSRC)

clean:
	rm -f matdep
	rm -f $(TESTOBJ)

init-project:
	mkdir -p ../src
	mkdir -p ../include
	cp -i examples/Makefile-example.txt ../Makefile
	cp -i examples/main.cpp ../src/
	cp -i examples/common.h ../include/
	cp -i examples/gitignore ../.gitignore
	make -C ../

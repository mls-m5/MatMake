
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
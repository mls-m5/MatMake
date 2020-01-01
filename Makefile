
CXXFLAGS = -Imatmake/src -Igoogletest/googlemock/include -Igoogletest/googletest/include -Igoogletest/googletest/
CXXFLAGS += -g

LIBS =  googletest/lib/libgtest.a googletest/lib/libgtest_main.a


all: gtest matmake file_test

.PHONY: gtest matmake

gtest:
	make -C googletest -s


matmake:
	make -C matmake/ debug

file_test: file_test.cpp matmake/src/*.cpp
	c++ file_test.cpp -o file_test ${CXXFLAGS} ${LIBS} -pthread -lstdc++fs


clean:
	rm -f *_test

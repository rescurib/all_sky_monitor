# Makefile for micro_planetarium tools

TARGETS = hello_star hello_bsc5

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I/usr/local/include -I/usr/include -I../../common/inc
LDFLAGS = -L/usr/local/lib -L/usr/lib -lsupernovas++ -lsupernovas -lcurl -lm -Wl,-rpath,/usr/local/lib

all: $(TARGETS)

hello_star: hello_star.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

hello_bsc5: hello_bsc5.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS)


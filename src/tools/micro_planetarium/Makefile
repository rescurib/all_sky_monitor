# Makefile for micro_planetarium tools

TARGETS = hello_star hello_bsc5 micro_planetarium gen_stars_csv

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I/usr/local/include -I/usr/include -I../../common/inc -I./include
LDFLAGS = -L/usr/local/lib -L/usr/lib -lsupernovas++ -lsupernovas -lcurl -lm -Wl,-rpath,/usr/local/lib

all: $(TARGETS)

hello_star: hello_star.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

hello_bsc5: hello_bsc5.cpp bsc5_parser.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

micro_planetarium: micro_planetarium.cpp bsc5_parser.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

gen_stars_csv: gen_stars_csv.cpp bsc5_parser.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS)


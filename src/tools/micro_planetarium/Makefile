# Makefile for hello_star

TARGET = hello_star
SRC = hello_star.cpp

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I/usr/local/include -I/usr/include
LDFLAGS = -L/usr/local/lib -L/usr/lib -lsupernovas++ -lsupernovas -lcurl -lm -Wl,-rpath,/usr/local/lib

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

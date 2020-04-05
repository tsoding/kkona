PKGS=sdl2 libpng
CXXFLAGS=-Wall -Wextra -Wconversion -Werror -O3 -pedantic -std=c++17 -fno-exceptions -ggdb $(shell pkg-config --cflags $(PKGS))
LIBS=$(shell pkg-config --libs $(PKGS)) -lm

kkona: main.cpp
	$(CXX) $(CXXFLAGS) -o kkona main.cpp $(LIBS)

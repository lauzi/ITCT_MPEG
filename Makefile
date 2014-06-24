GCC = g++
FLAGS = -std=c++11 -Wall -Wshadow -Wextra -O2 -lfreeglut -lopengl32 -lglew32

HFILES = file.hpp decoder.hpp huffman.hpp frame.hpp
CPPFILES = file.cpp decoder.cpp huffman.cpp frame.cpp

main.exe: main.cpp ${HFILES} ${CPPFILES}
	${GCC} ${CPPFILES} main.cpp -o main.exe ${FLAGS}

.PHONY = clear
clear:
	-rm -rf main.exe
GCC = g++
FLAGS = -std=c++11 -Wall -Wextra -g

HFILES = file.h decoder.h huffman.h
CPPFILES = file.cpp decoder.cpp huffman.cpp

main.exe: main.cpp ${HFILES} ${CPPFILES}
	${GCC} ${FLAGS} ${CPPFILES} main.cpp -o main.exe

.PHONY = clear
clear:
	-rm -rf main.exe
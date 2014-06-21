#include <cstdio>
#include <string>

#include "decoder.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        puts("Usage: ./main file_name.M1V");
        return -1;
    }

    std::string file_name(argv[1]);

    Decoder dec(file_name);

    try {
        dec.decode();
    } catch (std::string err) {
        printf("ERROR: %s\n", err.c_str());
    }

    return 0;
}

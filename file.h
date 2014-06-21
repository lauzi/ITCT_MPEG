#ifndef FILE_H
#define FILE_H

#include <cstdio>
#include <string>

#include "huffman.h"

const int default_buffer_size = 20;

typedef unsigned char uint8;
typedef unsigned int uint32;

class ReadOnlyFile {
  public:
    ReadOnlyFile (std::string file_name, int buffer_size = default_buffer_size);
    ~ReadOnlyFile ();

    uint32 read_bits(int n);
    uint32 peep_bits(int n);
    void goto_next_byte() { _j = 0; }

    int read_Huffman(Huffman *h);

  private:
    FILE *_file;

    int _bfr_siz, _mod;
    uint8 *_bfr;
    int _i, _j;
    bool _peeping, _has_read;

    void _read_bfr();
};

#endif
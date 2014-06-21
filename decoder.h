#ifndef DECODER_H
#define DECODER_H

#include <string>

#include "file.h"
#include "huffman.h"

typedef unsigned char uint8;
typedef char int8;
typedef unsigned int uint32;
typedef int int32;

class Decoder {
  public:
    Decoder (std::string file_name): _file(file_name) { _init_fucking_Huffman_tables(); }
    ~Decoder () {
        delete _MB_addr_incr_table;
        delete _cbp_table;
        delete _dct_coeff_table;
        for (int i = 1; i <= 3; ++i) delete _MB_type_tables[i];
        for (int i = 0; i < 2; ++i) delete _dc_len_tables[i];
    }

    void decode();
    void sequence_header();
    void extension_and_user_data();
    bool group_of_pictures();
    bool picture();
    bool slice(uint32 pct);
    void macroblock(uint32 pct);
    void block(int i, bool macroblock_intra);

  private:
    ReadOnlyFile _file;

    Huffman *_MB_addr_incr_table, *_cbp_table, *_dct_coeff_table;
    Huffman *_MB_type_tables[4], *_dc_len_tables[2];

    void _init_fucking_Huffman_tables();

};

#endif
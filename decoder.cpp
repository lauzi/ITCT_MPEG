#include "decoder.h"

#include <cstdio>
#include <cassert>


#include "huffman.h"

enum PCT { I=1, P, B, D };

void Decoder::decode() {
    sequence_header();
}


void Decoder::sequence_header() {
    if (_file.read_bits(32) != 0x000001b3)
        throw std::string("Expected SHC");

    uint32 hs = _file.read_bits(12);
    printf("HS = %d\n", hs);

    uint32 vs = _file.read_bits(12);
    printf("VS = %d\n", vs);

    uint32 par = _file.read_bits(4);
    printf("PAR = %d\n", par);

    uint32 pr = _file.read_bits(4);
    printf("PR = %d\n", pr);

    uint32 br = _file.read_bits(18);
    printf("BR = %d\n", br);

    uint32 mb = _file.read_bits(1);
    printf("MB = %d\n", mb);

    uint32 vbs = _file.read_bits(10);
    printf("VBS = %d\n", vbs);

    uint32 cpf = _file.read_bits(1);
    printf("CPF = %d\n", cpf);

    uint32 liqm = _file.read_bits(1);
    printf("LIQM = %d\n", liqm);
    if (liqm) {
        // load_intra_quantizer_matrix
    }

    uint32 lnqm = _file.read_bits(1);
    printf("LNQM = %d\n", lnqm);
    if (lnqm) {
        // load_non_intra_quantizer_matrix
    }

    _file.goto_next_byte();

    extension_and_user_data();

    while (group_of_pictures());
}

void Decoder::extension_and_user_data() {
    uint32 marker = _file.peep_bits(32);

    /*
    // ESC
    if (marker == ESC) {
        _file.read_bits(32);

        // read ESC

        _file.goto_next_byte();
        marker = _file.peep_bits(32);
    }

    // UDSC
    if (marker == UDSC) {
        _file.read_bits(32);

        _file.goto_next_byte();
    }
    */
}

bool Decoder::group_of_pictures() {
    if (_file.peep_bits(32) != 0x000001B8)
        return false;
    _file.read_bits(32);

    uint32 tc = _file.read_bits(25);
    printf("TC = %d\n", tc);

    uint32 cg = _file.read_bits(1);
    printf("CG = %d\n", cg);

    uint32 bl = _file.read_bits(1);
    printf("BL = %d\n", bl);

    _file.goto_next_byte();

    extension_and_user_data();

    while (picture());

    return true;
}

bool Decoder::picture() {
    if (_file.peep_bits(32) != 0x00000100)
        return false;
    _file.read_bits(32);

    uint32 tr = _file.read_bits(10);
    printf("TR = %d\n",  tr);

    uint32 pct = _file.read_bits(3);
    const static char pcts[] = "0IPBD";
    printf("PCT = %d, %c frame\n", pct, pcts[pct]);

    uint32 vbd = _file.read_bits(16);
    printf("VBD = %d\n", vbd);

    if (pct == P or pct == B) {
        uint32 fpfv = _file.read_bits(1);
        printf("FPFV = %d\n", fpfv);

        uint32 ffc = _file.read_bits(3);
        printf("FFC = %d\n", ffc);

        if (pct == B) {
            uint32 fpbv = _file.read_bits(1);
            printf("FPBV = %d\n", fpbv);

            uint32 bfc = _file.read_bits(3);
            printf("BFC = %d\n", bfc);
        }
    }

    while (_file.read_bits(1)) {
        uint32 eip = _file.read_bits(8);
        printf("EIP = %d\n", eip);
    }

    _file.goto_next_byte();

    extension_and_user_data();

    while (slice(pct));

    return true;
}

bool Decoder::slice(uint32 pct) {
    uint32 next_bits_32 = _file.peep_bits(32);
    if (not (0x00000101 <= next_bits_32 and next_bits_32 <= 0x000001AF))
        return false;
    _file.read_bits(32);

    uint32 qs = _file.read_bits(5);
    printf("QS = %d\n", qs);

    while (_file.read_bits(1)) {
        uint32 eis = _file.read_bits(8);
        printf("EIS = %d\n", eis);
    }

    do macroblock(pct);
    while (_file.peep_bits(23) != 0);

    _file.goto_next_byte();

    return true;
}

inline
int extend(int val, int len) {
    static const int pows[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096,
                               8192, 16384, 32768, 65536};

    return val >= pows[len-1] ? val : val - pows[len] + 1;
}

void Decoder::macroblock(uint32 pct) {
    static int cnt = 0;

    printf("Macroblock #%d:\n", ++cnt);

    while (_file.peep_bits(11) == (0x0001 << 3 | 7)) {
        _file.read_bits(11);
        printf("MBS\n");
    }

    while (_file.peep_bits(11) == (0x0001 << 3 | 0)) {
        _file.read_bits(11);
        printf("MBE\n");
    }

    int mbai = _file.read_Huffman(_MB_addr_incr_table);
    printf("MBAI = %d\n", mbai);

    assert (1 <= pct and pct <= 3);
    int mbt = _file.read_Huffman(_MB_type_tables[pct]);
    printf("MBT = %d\n", mbt);

    bool macroblock_quant = mbt >> 4;
    bool macroblock_motion_forward = (mbt >> 3) & 1;
    bool macroblock_motion_backward = (mbt >> 2) & 1;
    bool macroblock_pattern = (mbt >> 1) & 1;
    bool macroblock_intra = mbt & 1;

    if (macroblock_quant) {
        int qs = _file.read_bits(5);
        printf("QS = %d\n", qs);
    }

    // motion_vectors(macroblock_motion_forward, macroblock_motion_backward);

    int cbp = 0;
    if (macroblock_pattern)
        cbp = _file.read_Huffman(_cbp_table);

    if (pct == I) cbp = 63;

    for (int i = 0; i < 6; ++i)
        if (cbp & (1 << (5 - i)))
            block(i, macroblock_intra);
}

void Decoder::block(int i, bool macroblock_intra) {
    printf("  BLOCK %d:\n", i);

    if (macroblock_intra) {
        int dct_dc_size = _file.read_Huffman(_dc_len_tables[i >= 4]);

        printf("    DCT_DC_SIZE = %d\n", dct_dc_size);
        printf("    DCT_DC_DIFF = %d\n", _file.peep_bits(dct_dc_size));

        int dct_dc_diff = extend(_file.read_bits(dct_dc_size), dct_dc_size);
    } else { // read dct_coeff_first
        int run, lvl;

        if (_file.peep_bits(1) == 1) {
            run = 0, lvl = 1;
            _file.read_bits(1);
        } else {
            int tmp = _file.read_Huffman(_dct_coeff_table);

            run = tmp >> 8, lvl = tmp & 0xFF;
            if (_file.read_bits(1)) lvl *= -1;
        }
    }

    while (_file.peep_bits(2) != 2) {
        int run, lvl;

        if (_file.peep_bits(6) == 0x1) { // escape
            _file.read_bits(6);

            run = _file.read_bits(6), lvl = _file.read_bits(8);

            if (lvl == 0x00)
                lvl = _file.read_bits(8);
            else if (lvl == 0x80)
                lvl = _file.read_bits(8) - 256;
            else if (lvl & 0x80)
                lvl -= 128;
        } else {
            int tmp = _file.read_Huffman(_dct_coeff_table);
            run = tmp >> 8, lvl = tmp & 0xFF;

            if (_file.read_bits(1))
                lvl *= -1;
        }

        printf("    RUN = %d, LVL = %d\n", run, lvl);
    }

    _file.read_bits(2);
}

void Decoder::_init_fucking_Huffman_tables() {
    _MB_addr_incr_table = new Huffman(true);
    {
        int ls[] = {1, 3, 3, 4, 4, 5, 5, 7, 7, 8, 8, 8, 8, 8, 8, 10, 10, 10, 10,
                    10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11};
         int vs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
                     19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33};
        for (unsigned i = 0; i < sizeof(ls)/sizeof(int); ++i)
            _MB_addr_incr_table->insert(ls[i], vs[i]);
    }

    _MB_type_tables[0] = NULL;
    _MB_type_tables[1] = new Huffman(true);
    {
        int ls[] = {1, 2};
        int vs[] = {1, 17};
        for (unsigned i = 0; i < 2; ++i)
            _MB_type_tables[1]->insert(ls[i], vs[i]);
    }

    _MB_type_tables[2] = new Huffman(true);
    {
        int ls[] = { 1, 2, 3, 5,  5,  5,  6};
        int vs[] = {10, 2, 8, 1, 26, 18, 17};
        for (unsigned i = 0; i < 7; ++i)
            _MB_type_tables[2]->insert(ls[i], vs[i]);
    }

    _MB_type_tables[3] = new Huffman(true);
    {
        int ls[] = { 2,  2, 3, 3, 4,  4, 5,  5,  6,  6,  6};
        int vs[] = {12, 14, 4, 6, 8, 10, 1, 30, 26, 22, 17};
        for (unsigned i = 0; i < sizeof(ls)/sizeof(int); ++i)
            _MB_type_tables[3]->insert(ls[i], vs[i]);
    }

    _cbp_table = new Huffman(true);
    {
        int ls[] = {3,
                    4, 4, 4, 4,
                    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                    6, 6, 6, 6,
                    7, 7, 7, 7, 7, 7, 7, 7,
                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
                    8, 8,
                    9, 9, 9, 9, 9, 9};
        int vs[] = {60, 4, 8, 16, 32, 12, 48, 20, 40, 28, 44, 52, 56, 1, 61, 2, 62, 24, 36, 3, 63,
                    5, 9, 17, 33, 6, 10, 18, 34, 7, 11, 19, 35, 13, 49, 21, 41, 14, 50, 22, 42, 15,
                    51, 23, 43, 25, 37, 26, 38, 29, 45, 53, 57, 30, 46, 54, 58, 31, 47, 55, 59, 27,
                    39};
        for (unsigned i = 0; i < sizeof(ls)/sizeof(int); ++i)
            _cbp_table->insert(ls[i], vs[i]);
    }

    _dc_len_tables[0] = new Huffman(false);
    {
        int ls[] = {2, 2, 3, 3, 3, 4, 5, 6, 7};
        int vs[] = {1, 2, 0, 3, 4, 5, 6, 7, 8};
        for (int i = 0; i < 9; ++i)
            _dc_len_tables[0]->insert(ls[i], vs[i]);
    }

    _dc_len_tables[1] = new Huffman(false);
    {
        int ls[] = {2, 2, 2, 3, 4, 5, 6, 7, 8};
        int vs[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};

        for (int i = 0; i < 9; ++i)
            _dc_len_tables[1]->insert(ls[i], vs[i]);
    }

    _dct_coeff_table = new Huffman(true);
    {
        int ls[] = {2, 2,
                    3,
                    4, 4,
                    5, 5, 5,
                    8, 8, 8, 8, 8, 8, 8, 8,
                    6, 6, 6, 6,
                    7, 7, 7, 7,
                    6,
                    10, 10, 10, 10, 10, 10, 10, 10,
                    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
                    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
                    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
                    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
                    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};
        int vs[] = {1, 0, 257, 513, 2, 769, 1025, 3, 2561, 5, 259, 770, 2817, 3073, 6, 3329, 1281,
                    258, 1537, 1793, 2049, 4, 2305, 514, 2056, 1026, 3585, 3841, 260, 515, 7, 1282,
                    4097, 4353, 1538, 8, 771, 261, 4609, 4865, 9, 5121, 5377, 1794, 516, 10, 1027,
                    2050, 11, 5633, 5889, 6145, 6401, 6657, 12, 13, 14, 15, 262, 263, 517, 772, 1283,
                    2306, 2562, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 264,
                    265, 266, 267, 268, 269, 270, 32, 33, 34, 35, 36, 37, 38, 39, 40, 6913, 7169,
                    7425, 7681, 7937, 2818, 3074, 3330, 3586, 3842, 4098, 1539, 271, 272, 273, 274};

        for (unsigned i = 0; i < sizeof(ls)/sizeof(int); ++i)
            _dct_coeff_table->insert(ls[i], vs[i]);
    }

    /*
    for (int i = 0; i < 256; ++i) {
        if (tbl->tbls[0][i] < 0)
            printf("%X -> tbl %d\n", i, -tbl->tbls[0][i]);
        else
            printf("%X -> run=%d, lvl=%d\n", i, (tbl->tbls[0][i] >> 8) & 0xFF,
                                                (tbl->tbls[0][i])       & 0xFF);
    }
    */
}
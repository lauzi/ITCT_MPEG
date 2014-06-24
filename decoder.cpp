#include "decoder.hpp"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <cassert>

#include "huffman.hpp"

#define DEBUG

const bool output_to_file = true;


const double M_SQRT2 = sqrt(2);
const double M_PI = 4 * atan(1);


enum PCT { I=1, P, B, D };

void Decoder::decode() {
    sequence_header();
}

void Decoder::sequence_header() {
    if (_read_bits(32) != 0x000001b3)
        throw std::string("Expected SHC");

    debug("SEQUENCE HEADER:\n");

    uint32 hs = _read_bits(12);
    debug("  HS = %d\n", hs);
    horizontal_size = hs;

    uint32 vs = _read_bits(12);
    debug("  VS = %d\n", vs);
    vertical_size = vs;

    mb_width = (horizontal_size + 15) / 16;
    mb_height = (vertical_size + 15) / 16;

    _read_bits(4+4+18+1);
    _read_bits(10+1);

    uint32 liqm = _read_bits(1);
    debug("  LIQM = %d\n", liqm);
    if (liqm) {
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 8; ++j)
                intra_quant[i][j] = _read_bits(8);
    } else
        _init_intra_quant();

    uint32 lnqm = _read_bits(1);
    debug("  LNQM = %d\n", lnqm);
    if (lnqm) {
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 8; ++j)
                non_intra_quant[i][j] = _read_bits(8);
    } else
        _init_non_intra_quant();

    _goto_next_byte();

    extension_and_user_data();

    while (group_of_pictures());

    _output(_backward_frame);
}

bool Decoder::group_of_pictures() {
    if (_peep_bits(32) != 0x000001B8)
        return false;
    _read_bits(32);

    debug("  GROUP OF PICTURES:\n");

    _read_bits(27);

    _goto_next_byte();

    extension_and_user_data();

    while (picture());

    return true;
}

void Decoder::extension_and_user_data() {
    uint32 marker = _peep_bits(32);

    if (marker == 0x000001B5) { // ESC
        _read_bits(32);

        while (_peep_bits(24) != 0x000001) _read_bits(8);

        marker = _peep_bits(32);
    }

    if (marker == 0x000001B2) { // UDSC
        _read_bits(32);

        while (_peep_bits(24) != 0x000001) _read_bits(8);
    }
}

bool Decoder::picture() {
    if (_peep_bits(32) != 0x00000100)
        return false;
    _read_bits(32);

    _frame_count += 1;

    debug("    PICTURE:\n");

    uint32 tr = _read_bits(10);
    debug("      TR = %d\n",  tr);
    temporal_reference = tr;

    picture_coding_type = _read_bits(3);
    const static char pcts[] = "0IPBDabcdefghij";
    debug("      PCT = %d, %c frame\n", picture_coding_type, pcts[picture_coding_type]);

    if (picture_coding_type != I and picture_coding_type != P and picture_coding_type != B) {
        _read_bits(3);
        while (_peep_bits(24) != 0x000001) _read_bits(8);
        return true;
    }

    _read_bits(16);

    if (picture_coding_type == P or picture_coding_type == B) {
        uint32 fpfv = _read_bits(1);
        debug("      FPFV = %d\n", fpfv);
        full_pel_forward_vector = fpfv;

        uint32 ffc = _read_bits(3);
        debug("      FFC = %d\n", ffc);
        forward_r_size = ffc - 1;
        forward_f = 1 << forward_r_size;

        if (picture_coding_type == B) {
            uint32 fpbv = _read_bits(1);
            debug("      FPBV = %d\n", fpbv);
            full_pel_backward_vector = fpbv;

            uint32 bfc = _read_bits(3);
            debug("      BFC = %d\n", bfc);
            backward_r_size = bfc - 1;
            backward_f = 1 << backward_r_size;
        }
    }

    while (_read_bits(1)) _read_bits(8);

    _goto_next_byte();

    extension_and_user_data();

    if (picture_coding_type == I or picture_coding_type == P) {
        std::swap(_backward_frame, _forward_frame);
        _output(_forward_frame);
    }

    if (_current_frame == nullptr)
        _current_frame = new Frame(vertical_size, horizontal_size);

    macroblock_address = 0;
    while (slice());

    if (picture_coding_type == I or picture_coding_type == P)
        std::swap(_current_frame, _backward_frame);
    else if (picture_coding_type == B)
        _output(_current_frame);

    return true;
}

bool Decoder::slice() {
    uint32 next_bits_32 = _peep_bits(32);
    if (not (0x00000101 <= next_bits_32 and next_bits_32 <= 0x000001AF))
        return false;
    _read_bits(24);

    macroblock_address = (_read_bits(8) - 1) * mb_width - 1;

    debug("      SLICE:\n");

    uint32 qs = _read_bits(5);
    debug("        QS = %d\n", qs);
    quantizer_scale = qs;

    while (_read_bits(1)) _read_bits(8);

    past_intra_address = -2;
    recon_right_for = recon_right_for_prev = 0;
    recon_down_for = recon_down_for_prev = 0;
    recon_right_back = recon_right_back_prev = 0;
    recon_down_back = recon_down_back_prev = 0;

    do macroblock();
    while (_peep_bits(23) != 0);

    _goto_next_byte();

    return true;
}

void Decoder::macroblock() {
    while (_peep_bits(11) == (0x0001 << 3 | 7)) // useless
        _read_bits(11);

    int mbai = 0;

    while (_peep_bits(11) == (0x0001 << 3 | 0)) {
        _read_bits(11);
        mbai += 33;
        debug("        MBE\n");
    }

    mbai += _read_Huffman(_MB_addr_incr_table);
    debug("        MBAI = %d\n", mbai);

    _gg_skipped_macroblocks(mbai-1);

    macroblock_address += mbai;
    mb_row = macroblock_address / mb_width;
    mb_column = macroblock_address % mb_width;

    debug("        Macroblock #%d:\n", macroblock_address + 1);

    int mbt = _read_Huffman(_MB_type_tables[picture_coding_type]);
    debug("          MBT = %d\n", mbt);
    macroblock_quant = mbt >> 4;
    macroblock_motion_forward = (mbt >> 3) & 1;
    macroblock_motion_backward = (mbt >> 2) & 1;
    macroblock_pattern = (mbt >> 1) & 1;
    macroblock_intra = mbt & 1;

    if (macroblock_quant) {
        int qs = _read_bits(5);
        debug("          QS = %d\n", qs);
        quantizer_scale = qs;
    }

    if (macroblock_motion_forward) {
        recon_right_for = _read_motion_vector(forward_f, forward_r_size,
                                                  full_pel_forward_vector, recon_right_for_prev);

        recon_down_for = _read_motion_vector(forward_f, forward_r_size,
                                                 full_pel_forward_vector, recon_down_for_prev);

        debug("            ForMV = (%d, %d)\n", recon_right_for, recon_down_for);
    } else {
        recon_right_for = recon_right_for_prev = 0;
        recon_down_for = recon_down_for_prev = 0;
    }

    if (macroblock_motion_backward) {
        recon_right_back = _read_motion_vector(backward_f, backward_r_size,
                                                   full_pel_backward_vector, recon_right_back_prev);

        recon_down_back = _read_motion_vector(backward_f, backward_r_size,
                                                  full_pel_backward_vector, recon_down_back_prev);

        debug("            BackMV = (%d, %d)\n", recon_right_back, recon_down_back);
    } else {
        recon_right_back = recon_right_back_prev = 0;
        recon_down_back = recon_down_back_prev = 0;
    }

    int cbp = macroblock_intra ? 63 : 0;
    if (macroblock_pattern)
        cbp = _read_Huffman(_cbp_table);

    _process_macroblock(cbp);

    if (macroblock_intra)
        past_intra_address = macroblock_address;
}

bool Decoder::_read_coeff(int *run, int *lvl, bool first = false) {
    int tmp = _peep_bits(2);

    if (tmp & 2) {
        if (first) {
            _read_bits(2);
            *run = 0, *lvl = (tmp & 1) ? -1 : 1;
        } else {
            if (tmp == 2) {
                _read_bits(2);
                return false;
            }

            *run = 0, *lvl = _read_bits(3) == 7 ? -1 : 1;
        }
        return true;
    }

    if (_peep_bits(6) == 1) { // escape
        _read_bits(6);

        *run = _read_bits(6), *lvl = _read_bits(8);

        if (*lvl == 0x00)
            *lvl = _read_bits(8);
        else if (*lvl == 0x80)
            *lvl = _read_bits(8) - 256;
        else if (*lvl & 0x80)
            *lvl -= 256;
        return true;
    }

    int input = _read_Huffman(_dct_coeff_table);
    *run = input >> 8, *lvl = input & 0xFF;
    if (_read_bits(1)) *lvl *= -1;
    return true;
}

inline
int sign(int n) { return n > 1 ? 1 : -1; }

void Decoder::block(int i) {
    debug("          BLOCK %d:\n", i);

    int idx = 0;
    bool coeff_first = false;

    if (macroblock_intra) {
        int dct_dc_size = _read_Huffman(_dc_len_tables[i >= 4]);
        debug("            DCT_DC_SIZE = %d\n", dct_dc_size);

        if (dct_dc_size > 0) {
            int dct_dc_diff = _read_bits(dct_dc_size);

            debug("            DCT_DC_DIFF = %d\n", dct_dc_diff);

            if (dct_dc_diff & (1 << (dct_dc_size-1)))
                dct_recon[0][0] = dct_dc_diff;
            else
                dct_recon[0][0] = (-1 << dct_dc_size) | (dct_dc_diff+1);
        } else dct_recon[0][0] = 0;
    } else { // read dct_coeff_first
        idx = -1, coeff_first = true;
    }

    for (int run, lvl; _read_coeff(&run, &lvl, coeff_first); coeff_first = false) {
        debug("            RUN = %d, LVL = %d\n", run, lvl);

        idx += run + 1;
        dct_recon[i_scan_y[idx]][i_scan_x[idx]] = lvl;
    }

    if (macroblock_intra) {
        int *dct_dc_past_p = &dct_dc_y_past;
        bool first = i == 0;
        if (i >= 4) {
            first = true;
            dct_dc_past_p = i == 4 ? &dct_dc_cb_past : &dct_dc_cr_past;
        }

        _process_intra_block(dct_dc_past_p, first);
    } else {
        _process_non_intra_block();
    }

    _idct();
}

void Decoder::_process_intra_block(int *dct_dc_past_p, bool first) {
    int dc_val;

    if (first and past_intra_address+1 < macroblock_address)
        *dct_dc_past_p = 1024;
    dc_val = *dct_dc_past_p + (dct_recon[0][0] << 3);
    *dct_dc_past_p = dc_val;

    for (int m = 0; m < 8; ++m) {
        for (int n = 0; n < 8; ++n) {
            int &dct_val = dct_recon[m][n];

            dct_val = (dct_val * quantizer_scale * intra_quant[m][n]) >> 3;
            if ((dct_val & 1) == 0) dct_val -= sign(dct_val);
            if (dct_val > 2047) dct_val = 2047;
            if (dct_val < -2048) dct_val = -2048;
        }
    }

    dct_recon[0][0] = dc_val;
}

void Decoder::_process_non_intra_block() {
    for (int m = 0; m < 8; ++m) {
        for (int n = 0; n < 8; ++n) {
            int &dct_val = dct_recon[m][n];

            if (dct_val == 0) continue;

            int sgn = sign(dct_val);

            dct_val = (((dct_val << 1) + sgn) * quantizer_scale * non_intra_quant[m][n]) >> 4;
            if ((dct_val & 1) == 0) dct_val -= sgn;
            if (dct_val > 2047) dct_val = 2047;
            if (dct_val < -2048) dct_val = -2048;
        }
    }
}

int Decoder::_read_motion_vector(int f, int r_size, int full_pel_vector, int &recon) {
    int32 mc = _read_Huffman(_motion_vector_table);
    if (mc != 0) mc *= _read_bits(1) ? -1 : 1;

    int32 mr = (f == 1 or mc == 0) ? 0 : _read_bits(r_size);

    return _gg_motion_vectors(f, mc, mr, full_pel_vector, recon);
}

int Decoder::_gg_motion_vectors(int f, int motion_code, int motion_r, bool full_pel_vector,
                                 int &recon_prev) {
    int complement_r = (f == 1 or motion_code == 0) ? 0 : f - 1 - motion_r;

    int little = motion_code * f, big;
    if (little == 0)
        big = 0;
    else if (little > 0)
        little -= complement_r, big = little - (f << 5);
    else
        little += complement_r, big = little + (f << 5);

    int max = (f<<4), min = -max;
    int new_vector = recon_prev + little;

    int recon = recon_prev + (new_vector < max and new_vector >= min ? little : big);
    recon_prev = recon;
    if (full_pel_vector) recon <<= 1;

    return recon;
}

void Decoder::_init_fucking_Huffman_tables_fuck() {
    _MB_addr_incr_table = new Huffman(true);
    {
        int ls[] = {1, 3, 3, 4, 4, 5, 5, 7, 7, 8, 8, 8, 8, 8, 8, 10, 10, 10, 10,
                    10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11};
         int vs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
                     19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33};
        for (unsigned i = 0; i < sizeof(ls)/sizeof(int); ++i)
            _MB_addr_incr_table->insert(ls[i], vs[i]);
    }

    _MB_type_tables[0] = nullptr;
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
        int vs[] = {14, 12, 6, 4, 10, 8, 1, 30, 26, 22, 17};
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

    _motion_vector_table = new Huffman(true);
    {
        int ls[] = {1,
                    2,
                    3,
                    4,
                    6,
                    7, 7, 7,
                    9, 9, 9,
                    10, 10, 10, 10, 10, 10};
        int vs[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        for (unsigned i = 0; i < sizeof(ls)/sizeof(int); ++i)
            _motion_vector_table->insert(ls[i], vs[i]);
    }
}

void Decoder::_init_intra_quant() {
    static const int32 vals[] =
        {8,	16,	19,	22,	26,	27,	29,	34,
        16,	16,	22,	24,	27,	29,	34,	37,
        19,	22,	26,	27,	29,	34,	34,	38,
        22,	22,	26,	27,	29,	34,	37,	40,
        22,	26,	27,	29,	32,	35,	40,	48,
        26,	27,	29,	32,	35,	40,	48,	58,
        26,	27,	29,	34,	38,	46,	56,	69,
        27, 29, 35,	38,	46,	56,	69,	83};

    memcpy(intra_quant, vals, 64 * sizeof(int));
}

void Decoder::_init_non_intra_quant() {
    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 8; ++j)
            non_intra_quant[i][j] = 16;
}

void Decoder::_init_scan() {
    static const int vals[] = {
        0,	1,	5,	6,	14,	15,	27,	28,
        2,	4,	7,	13,	16,	26,	29,	42,
        3,	8,	12,	17,	25,	30,	41,	43,
        9,	11,	18,	24,	31,	40,	44,	53,
        10,	19,	23,	32,	39,	45,	52,	54,
        20,	22,	33,	38,	46,	51,	55,	60,
        21,	34,	37,	47,	50,	56,	59,	61,
        35,	36,	48,	49,	57,	58,	62,	63};

    memcpy(scan, vals, 64 * sizeof(int));

    for (int y = 0, idx = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x, ++idx) {
            i_scan_y[vals[idx]] = y;
            i_scan_x[vals[idx]] = x;
        }
    }
}


// http://www.reznik.org/papers/SPIE07_MPEG-C_IDCT.pdf
void fast_idct(double &o0, double &o1, double &o2, double &o3,
               double &o4, double &o5, double &o6, double &o7) {
    double a0 = o0, a1 = o4, a2 = o2, a3 = o6;
    double a4 = o1 - o7, a5 = o3 * M_SQRT2, a6 = o5 * M_SQRT2, a7 = o1 + o7;

    static const double a_angle = 3.0 * M_PI / 8;
    static const double a = M_SQRT2 * cos(a_angle), b = M_SQRT2 * sin(a_angle);
    double b0 = a0 + a1, b1 = a0 - a1;
    double b2 = a2 * a - a3 * b, b3 = a2 * b + a3 * a;
    double b4 = a4 + a6, b5 = a7 - a5, b6 = a4 - a6, b7 = a7 + a5;

    static const double b_angle = M_PI / 16;
    static const double d = cos(b_angle), e = sin(b_angle);
    static const double c_angle = a_angle / 2;
    static const double n = cos(c_angle), t = sin(c_angle);

    double c0 = b0 + b3, c1 = b1 + b2, c2 = b1 - b2, c3 = b0 - b3;
    double c4 = b4 * n - b7 * t, c5 = b5 * d - b6 * e;
    double c6 = b6 * d + b5 * e, c7 = b7 * n + b4 * t;

    o0 = c0 + c7, o1 = c1 + c6, o2 = c2 + c5, o3 = c3 + c4;
    o4 = c3 - c4, o5 = c2 - c5, o6 = c1 - c6, o7 = c0 - c7;
}

void Decoder::_idct() {
    double tmp[8][8];
    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 8; ++j)
            tmp[i][j] = dct_recon[i][j];

    for (int i = 0; i < 8; ++i)
        fast_idct(tmp[i][0], tmp[i][1], tmp[i][2], tmp[i][3],
                  tmp[i][4], tmp[i][5], tmp[i][6], tmp[i][7]);

    for (int i = 0; i < 8; ++i)
        fast_idct(tmp[0][i], tmp[1][i], tmp[2][i], tmp[3][i],
                  tmp[4][i], tmp[5][i], tmp[6][i], tmp[7][i]);

    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 8; ++j)
            dct_recon[i][j] = tmp[i][j] / 8;
}

void Decoder::_clip_range() {
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            if (dct_recon[i][j] < 0) dct_recon[i][j] = 0;
            if (dct_recon[i][j] > 255) dct_recon[i][j] = 255;
        }
    }
}

int Decoder::debug(const char *format, ...) {
    return 0;
    va_list arg;
    int length = 0;

    if (_frame_count == 18 or _frame_count == 0) {
        va_start(arg, format);
        length = vprintf(format, arg);
        va_end(arg);
    }

    return length;
}

void Decoder::_output(Frame *frame) {
    if (frame == nullptr) return ;

    if (output_to_file) {
        char file_name[120];
        sprintf(file_name, "output/frame_%03d.bmp", ++_output_frame);
        frame->output_to_file(std::string(file_name));
    }
}

void Decoder::_gg_skipped_macroblocks(int n) {
    for (int i = 1; i <= n; ++i) {
        int block_num = macroblock_address + i;

        mb_row = block_num / mb_width;
        mb_column = block_num % mb_width;

        macroblock_quant = false;
        macroblock_pattern = false;
        macroblock_intra = false;

        if (picture_coding_type == P) {
            recon_right_for = recon_right_for_prev = 0;
            recon_down_for = recon_down_for_prev = 0;

            _process_macroblock(0);

        } else if (picture_coding_type == B) {
            _process_macroblock(0);
        }
    }
}

void Decoder::_process_macroblock(int cbp) {
    for (int i = 0; i < 6; ++i) {
        memset(dct_recon, 0, 64 * sizeof(int32));

        if (cbp & (1 << (5 - i))) block(i);

        if (macroblock_intra) {
            // pass
        } else if (picture_coding_type == P) {
            _forward_frame->add_macroblock(dct_recon, mb_row, mb_column, i,
                                           recon_right_for, recon_down_for, false);
        } else if (picture_coding_type == B) {
            _forward_frame->add_macroblock(dct_recon, mb_row, mb_column, i,
                                           recon_right_for, recon_down_for, true);
            _backward_frame->add_macroblock(dct_recon, mb_row, mb_column, i,
                                            recon_right_back, recon_down_back, true);
        }

        _clip_range();

        _current_frame->set_macroblock(dct_recon, mb_row, mb_column, i);
    }
}
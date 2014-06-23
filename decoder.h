#ifndef DECODER_H
#define DECODER_H

#include <string>
#include <cstdarg>

#include "file.h"
#include "huffman.h"
#include "frame.h"


typedef unsigned char uint8;
typedef char int8;
typedef unsigned int uint32;
typedef int int32;

class Decoder {
  public:
    Decoder (std::string file_name): _frame_count(0), _file(file_name), _output_frame(0) {
        _init_fucking_Huffman_tables_fuck();
        _init_scan();
        _forward_frame = _current_frame = _backward_frame = NULL;
    }
    ~Decoder () {
        delete _MB_addr_incr_table;
        delete _cbp_table;
        delete _dct_coeff_table;
        for (int i = 1; i <= 3; ++i) delete _MB_type_tables[i];
        for (int i = 0; i < 2; ++i) delete _dc_len_tables[i];

        delete _forward_frame;
        delete _current_frame;
        delete _backward_frame;
    }

    void decode();
    void sequence_header();
    void extension_and_user_data();
    bool group_of_pictures();
    bool picture();
    bool slice();
    void macroblock();
    void block(int i);

  private:
    int _frame_count;

    ReadOnlyFile _file;

    uint32 _read_bits(int n) { return _file.read_bits(n); }
    uint32 _peep_bits(int n) { return _file.peep_bits(n); }
    uint32 _read_Huffman(Huffman *h) { return _file.read_Huffman(h); }
    void _goto_next_byte() { _file.goto_next_byte(); }

    bool _read_coeff(int *run, int *lvl, bool first); // false if EOB


    Huffman *_MB_addr_incr_table, *_cbp_table, *_dct_coeff_table, *_motion_vector_table;
    Huffman *_MB_type_tables[4], *_dc_len_tables[2];

    int32 scan[8][8];
    int32 i_scan_y[64], i_scan_x[64];

    void _init_fucking_Huffman_tables_fuck();
    void _init_intra_quant();
    void _init_non_intra_quant();
    void _init_scan();

    void _process_intra_block(int *dct_dc_past_p, bool first);
    void _process_non_intra_block();

    void _gg_skipped_macroblocks(int n);
    void _process_macroblock(int cbp);

    int32 recon_right_for_prev, recon_down_for_prev;
    int32 recon_right_back_prev, recon_down_back_prev;
    int32 _gg_motion_vectors(int f, int motion_code, int motion_r, bool full_pel_vector,
                             int &recon_prev);

    int _read_motion_vector(int f, int r_size, int full_pel_vector, int &recon);

    void _idct();
    void _clip_range();

    int _output_frame;
    void _output(Frame *frame);

    // sequence
    uint32 horizontal_size, vertical_size;
    uint32 mb_width, mb_height;
    int32 intra_quant[8][8];
    int32 non_intra_quant[8][8];

    // picture layer
    uint32 picture_coding_type;
    uint32 temporal_reference;
    uint32 full_pel_forward_vector, forward_r_size, forward_f;
    uint32 full_pel_backward_vector, backward_r_size, backward_f;

    // quantizer_scale?
    int32 quantizer_scale;
    int32 past_intra_address;
    int32 dct_dc_y_past, dct_dc_cb_past, dct_dc_cr_past;

    // macroblock
    int32 macroblock_address;
    bool macroblock_quant;
    bool macroblock_motion_forward;
    bool macroblock_motion_backward;
    bool macroblock_pattern;
    bool macroblock_intra;

    int32 recon_right_for, recon_down_for;
    int32 recon_right_back, recon_down_back;

    uint32 mb_row, mb_column;

    // block
    int32 dct_recon[8][8];

    Frame *_forward_frame, *_current_frame, *_backward_frame;

    int debug(const char *format, ...);
};

#endif
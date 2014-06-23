#ifndef FRAME_H
#define FRAME_H

#include <string>
#include <vector>


typedef unsigned char uint8;
typedef int int32;

typedef int32 bv_t;

class Frame {
  public:
    Frame (int height, int width):
        _h(height), _w(width),
        _mbh((height + 15) / 16), _mbw((width + 15) / 16), _lw(_mbw * 8) {

        _y.resize(_mbh * _mbw * 16 * 16);
        _cb.resize(_mbh * _mbw * 8 * 8);
        _cr.resize(_mbh * _mbw * 8 * 8);
    }

    void set_macroblock(bv_t vals[8][8], int my, int mx, int i) {
        if (i < 4)
            _set_macroblock_y(vals, my, mx, i);
        else
            _set_macroblock_c(vals, my, mx, i == 4 ? _cb : _cr);
    }

    void add_macroblock(bv_t vals[8][8], int my, int mx, int i,
                          int recon_right, int recon_down, bool bblock) {
        if (i < 4)
            _add_macroblock_y(vals, my, mx, i, recon_right, recon_down, bblock);
        else
            _add_macroblock_c(vals, my, mx, recon_right, recon_down, bblock, i == 4 ? _cb : _cr);
    }

    void output_to_file(std::string file_name);

  private:
    int _h, _w;
    int _mbh, _mbw;
    int _lw;

    void _set_macroblock_y(bv_t vals[8][8], int my, int mx, int i);
    void _set_macroblock_c(bv_t vals[8][8], int my, int mx, std::vector<uint8> &v);

    void _add_macroblock_y(bv_t vals[8][8], int my, int mx, int i,
                           int recon_right, int recon_down, bool bblock);
    void _add_macroblock_c(bv_t vals[8][8], int my, int mx,
                           int rr, int rd, bool bblock, std::vector<uint8> &v);

    std::vector<uint8> _y, _cb, _cr;
};

#endif
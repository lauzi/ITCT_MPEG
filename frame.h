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

    void set_macroblock_y(bv_t vals[8][8], int my, int mx, int i);
    void set_macroblock_cb(bv_t vals[8][8], int my, int mx);
    void set_macroblock_cr(bv_t vals[8][8], int my, int mx);

    // void get_macroblock_y(bv_t vals[8][8], int y, int x);

    void output_to_file(std::string file_name);

  private:
    int _h, _w;
    int _mbh, _mbw;
    int _lw;

    std::vector<uint8> _y, _cb, _cr;
};

#endif
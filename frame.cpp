#include "frame.h"

#include <cassert>


#include "bmpwriter.h"

void Frame::set_macroblock_y(bv_t vals[8][8], int my, int mx, int i) {
    int base_y = my << 4, base_x = mx << 4;
    if (i & 1) base_x += 8;
    if (i & 2) base_y += 8;

    for (int i = 0; i < 8; ++i) {
        int idx = (base_y + i) * _lw * 2 + base_x;
        for (int j = 0; j < 8; ++j, ++idx)
            _y[idx] = vals[i][j];
        assert (idx <= _mbh * _mbw * 16 * 16);
    }
}

void Frame::set_macroblock_cb(bv_t vals[8][8], int my, int mx) {
    int base_y = my << 3, base_x = mx << 3;

    for (int i = 0; i < 8; ++i) {
        int idx = (base_y + i) * _lw + base_x;
        for (int j = 0; j < 8; ++j, ++idx)
            _cb[idx] = vals[i][j];
        assert (idx <= _mbh * _mbw * 8 * 8);
    }
}

void Frame::set_macroblock_cr(bv_t vals[8][8], int my, int mx) {
    int base_y = my << 3, base_x = mx << 3;

    for (int i = 0; i < 8; ++i) {
        int idx = (base_y + i) * _lw + base_x;
        for (int j = 0; j < 8; ++j, ++idx)
            _cr[idx] = vals[i][j];
        assert (idx <= _mbh * _mbw * 8 * 8);
    }
}

void Frame::output_to_file(std::string file_name) {
    BMPWriter writer(_h, _w, file_name.c_str());

    for (int i = 0, idx = 0; i < _h; ++i) {
        for (int j = 0; j < _w; ++j, ++idx) {
            int cidx = ((_lw * i) >> 2) + (j >> 1);
            writer.write_pxl(_y[idx], _cb[cidx], _cr[cidx]);
        }
    }
}

#include "frame.h"

#include <cassert>


#include "bmpwriter.h"

void Frame::_set_macroblock_y(bv_t vals[8][8], int my, int mx, int i) {
    int base_y = my << 4, base_x = mx << 4;
    if (i & 1) base_x += 8;
    if (i & 2) base_y += 8;

    for (int i = 0; i < 8; ++i) {
        int idx = (base_y + i) * (_lw * 2) + base_x;
        for (int j = 0; j < 8; ++j, ++idx)
            _y[idx] = vals[i][j];
        assert (idx <= _mbh * _mbw * 16 * 16);
    }
}

void Frame::_set_macroblock_c(bv_t vals[8][8], int my, int mx, std::vector<uint8> &v) {
    int base_y = my << 3, base_x = mx << 3;

    for (int i = 0; i < 8; ++i) {
        int idx = (base_y + i) * _lw + base_x;
        for (int j = 0; j < 8; ++j, ++idx)
            v[idx] = vals[i][j];
        assert (idx <= _mbh * _mbw * 8 * 8);
    }
}

void Frame::_add_macroblock_y(bv_t vals[8][8], int my, int mx, int ii,
                             int recon_right, int recon_down, bool bblock) {
    int right = recon_right >> 1, right_half = recon_right & 1;
    int down = recon_down >> 1, down_half = recon_down & 1;

    const int lw = _lw * 2;
    int y = my << 4, x = mx << 4;
    if (ii & 1) x |= 8;
    if (ii & 2) y |= 8;
    y += down, x += right;

    for (int i = 0; i < 8; ++i) {
        int idx = (y + i) * lw + x;
        for (int j = 0; j < 8; ++j, ++idx) {
            int sum = _y[idx], lg_div = bblock ? 1 : 0;
            if (down_half)
                sum += _y[idx + lw], lg_div += 1;
            if (right_half)
                sum += _y[idx + 1], lg_div += 1;
            if (right_half and down_half)
                sum += _y[idx + lw + 1];
            vals[i][j] += sum >> lg_div;
        }
    }
}

void Frame::_add_macroblock_c(bv_t vals[8][8], int my, int mx, int rr, int rd, bool bblock,
                              std::vector<uint8> &v) {
    rr >>= 1, rd >>= 1;
    int right = rr >> 1, right_half = rr & 1;
    int down = rd >> 1, down_half = rd & 1;

    int y = my<<3, x = mx<<3;
    y += down, x += right;

    for (int i = 0; i < 8; ++i) {
        int idx = (y + i) * _lw + x;
        for (int j = 0; j < 8; ++j, ++idx) {
            int sum = v[idx], lg_div = bblock ? 1 : 0;
            if (down_half)
                sum += v[idx + _lw], lg_div += 1;
            if (right_half)
                sum += v[idx + 1], lg_div += 1;
            if (right_half and down_half)
                sum += v[idx + _lw + 1];
            vals[i][j] += sum >> lg_div;
        }
    }
}

void Frame::output_to_file(std::string file_name) {
    BMPWriter writer(_h, _w, file_name.c_str());

    for (int i = 0; i < _h; ++i) {
        int idx = i * (_lw * 2);
        for (int j = 0; j < _w; ++j, ++idx) {
            int cidx = (i >> 1) * _lw + (j >> 1);
            writer.write_pxl(_y[idx], _cb[cidx], _cr[cidx]);
        }
    }
}

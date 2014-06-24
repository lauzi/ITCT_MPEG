#include "frame.hpp"

#include <cstdio>
#include <cmath>
#include <cassert>

typedef short int16;

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

void Frame::calc_bmp() {
    if (not _changed) return ;
    _changed = false;

    int idx = 0;
    for (int i = 0; i < _h; ++i) {
        for (int j = 0; j < _w; ++j) {
            const uint8 y = _y[i * _lw * 2 + j];
            const uint8 cb = _cb[(i>>1) * _lw + (j>>1)];
            const uint8 cr = _cr[(i>>1) * _lw + (j>>1)];
            _write_pel(y, cb, cr, idx);
            idx += 3;
        }
        idx += 3 & (4 - (idx & 3));
    }
}

void Frame::_write_pel(double y, double cb, double cr, int idx) {
    y *= 298.082;
    y /= 256, cb /= 256, cr /= 256;
    double rgb[] = {
        y + 516.412 * cb                - 276.836,
        y - 100.291 * cb - 208.120 * cr + 135.576,
        y                + 408.583 * cr - 222.921
    };

    for (int i = 0; i < 3; ++i) {
        rgb[i] = round(rgb[i]);
        if (rgb[i] > 255) bmp[idx++] = 255;
        else if (rgb[i] < 0) bmp[idx++] = 0;
        else bmp[idx++] = rgb[i];
    }
}

void Frame::output_to_file(std::string file_name) {
    FILE *file = fopen(file_name.c_str(), "wb");
    if (file == NULL) throw std::string("Frame::Could not open output file");

    fwrite("BM", 1, 2, file);
    int bfOffBits = 2 + 4 + 2 + 2 + 4 + 40;
    int size = bfOffBits + bmp.size();
    int tmp = 0;

    fwrite(&size, 4, 1, file);
    fwrite(&tmp, 4, 1, file);
    fwrite(&bfOffBits, 4, 1, file);

    tmp = 40;

    fwrite(&tmp, 4, 1, file);
    fwrite(&_w, 4, 1, file);
    int neg_height = -_h;
    fwrite(&neg_height, 4, 1, file);

    int16 tmp16 = 1;
    fwrite(&tmp16, 2, 1, file);
    tmp16 = 24;
    fwrite(&tmp16, 2, 1, file);

    tmp = 0;
    for (int i = 0; i < 6; ++i)
        fwrite(&tmp, 4, 1, file);

    calc_bmp();
    fwrite(&bmp[0], 1, bmp.size(), file);

    fclose(file);
}

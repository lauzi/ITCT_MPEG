#include "bmpwriter.h"

#include <cmath>

typedef short int16;

void BMPWriter::write_pxl(double y, double cb, double cr) {
    y -= 16, cb -= 128, cr -= 128;

    y *= 298.082 / 256;
    double rgb[] = {
        y + 516.412 * cb / 256                      - 276.836,
        y - 100.291 * cb / 256 - 208.120 * cr / 256 + 135.576,
        y                      + 408.583 * cr / 256 - 222.921
    };

    int idx = _x++ * 3;
    for (int i = 0; i < 3; ++i) {
        rgb[i] = round(rgb[i]);
        if (rgb[i] > 255) _out_bfr[idx++] = 255;
        else if (rgb[i] < 0) _out_bfr[idx++] = 0;
        else _out_bfr[idx++] = rgb[i];
    }

    if (_x == width) {
        _write(_out_bfr, 1, _bfr_len);
        _x = 0;
    }
}

void BMPWriter::_write_header() {
    _write("BM", 1, 2);
    int bfOffBits = 2+4+2+2+4+40;
    int size = bfOffBits + height * _bfr_len;
    int tmp = 0;

    _write(&size, 4, 1);
    _write(&tmp, 4, 1);
    _write(&bfOffBits, 4, 1);

    tmp = 40;

    _write(&tmp, 4, 1);
    _write(&width, 4, 1);
    int tmp_height = -height;
    _write(&tmp_height, 4, 1);

    int16 tmp16 = 1;
    _write(&tmp16, 2, 1);
    tmp16 = 24;
    _write(&tmp16, 2, 1);

    tmp = 0;
    for (int i = 0; i < 6; ++i)
        _write(&tmp, 4, 1);
}

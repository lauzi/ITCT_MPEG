#ifndef BMPWRITER_H
#define BMPWRITER_H

#include <cstdio>

typedef unsigned char uint8;

class BMPWriter {
public:
    const int height, width;
    BMPWriter(int n_height, int n_width, const char *file_name):
        height(n_height), width(n_width),
        _bfr_len((3 * width + 3) / 4 * 4), _x(0),
        _out_bfr(NULL) {

        _OUT = fopen(file_name, "wb");
        if (_OUT == NULL) throw "BMPWriter::Could not open output file";

        _out_bfr = new uint8 [_bfr_len]();

        _write_header();
    }

    ~BMPWriter() { fclose(_OUT); delete [] _out_bfr; }

    void write_pxl(double y, double cb, double cr);
private:
    FILE *_OUT;
    const int _bfr_len;
    int _x;

    uint8 *_out_bfr;

    size_t _write(const void *ptr, size_t size, size_t count) {
        return fwrite(ptr, size, count, _OUT);
    }

    void _write_header();
};

#endif
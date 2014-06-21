#include "file.h"

#include <cassert>
#include <algorithm>

ReadOnlyFile::ReadOnlyFile (std::string file_name, int buffer_size) {
    assert (buffer_size >= 2);

    _file = fopen(file_name.c_str(), "rb");
    if (_file == NULL)
        throw "Could not open file: " + file_name;

    _bfr_siz = 1 << buffer_size;
    _mod = (_bfr_siz / 2) - 1;
    _bfr = new uint8 [_bfr_siz];
    _i = _bfr_siz-1, _j = 0;
    _peeping = _has_read = false;
}

ReadOnlyFile::~ReadOnlyFile () {
    fclose(_file);

    delete [] _bfr;
}

uint32 ReadOnlyFile::read_bits(int n) {
    assert (0 <= n and n <= 32);

    static uint32 bitmask[9] = {0};

    if (bitmask[1] == 0)
        for (int i = 1; i <= 8; ++i)
            bitmask[i] = bitmask[i-1] << 1 | 1;

    uint32 ans = 0;
    for (int dn; n > 0; n -= dn) {
        assert (_j >= 0);
        if (_j == 0) {
            _i += 1, _j = 8;
            if ((_i & _mod) == 0 and (_i != 0))
                _read_bfr();
        }

        dn = std::min(n, _j);
        _j -= dn;

        ans <<= dn;
        ans |= (_bfr[_i] >> _j) & bitmask[dn];
    }

    return ans;
}

uint32 ReadOnlyFile::peep_bits(int n) {
    int i = _i, j = _j;
    _peeping = true;
    uint32 ans = read_bits(n);
    _peeping = false, _i = i, _j = j;
    return ans;
}

void ReadOnlyFile::_read_bfr() {
    if (_i == _bfr_siz) _i = 0;

    if (not _has_read)
        fread(_bfr + _i, 1, _bfr_siz / 2, _file);

    _has_read = not _peeping;

    _j = 8;
}

int ReadOnlyFile::read_Huffman(Huffman *h) {
    for (int tbl = 0; ; ) {
        int res = h->tbls[tbl][peep_bits(8)];
        if (res < 0) {
            tbl = -res;
            read_bits(8);
        } else {
            read_bits(res >> 16);
            return res & 0xFFFF;
        }
    }

    assert (false);
    return -1;
}
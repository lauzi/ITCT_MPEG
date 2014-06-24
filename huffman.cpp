#include <cstdio>
#include "huffman.hpp"

void Huffman::insert(int len, uint16 val) {
    const int bitmask = _rev ? 255 : 0;

    if (len > 8) {
        if (_ins_tbl == 0) {
            for (int tbl = -1; _ins_idx < 256; ) {
                tbls.push_back(new int [256]);
                tbls[0][bitmask ^ (_ins_idx++)] = tbl--;
            }
        }

        len -= 8;
    }

    if (_ins_idx >= 256)
        _ins_tbl += 1, _ins_idx = 0, _unit = 1<<7;

    _unit = 1 << (8 - len);

    int new_idx = (len << 16) | val;
    for (int i = 0; i < _unit; ++i)
        tbls[_ins_tbl][bitmask ^ (_ins_idx++)] = new_idx;
}

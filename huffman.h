#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <string>
#include <vector>

typedef unsigned char uint8;
typedef unsigned short uint16;

class Huffman {
public:
    Huffman(bool rev = false): _rev(rev) { tbls.push_back(new int [256]); clear(); }
    ~Huffman() {
        for (const auto &ptr : tbls)
            delete [] ptr;
    }
    void insert(int len, uint16 val);

    void clear() { _ins_tbl = _ins_idx = 0, _unit = 1<<7; }

    std::vector<int*> tbls;
private:
    int _ins_tbl, _ins_idx, _unit;
    bool _rev;
};

#endif

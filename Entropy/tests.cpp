#include "entropy_core.h"
#include <iostream>
#include <fstream>
#include <iterator>
static void check(bool b) { if (!b) throw std::runtime_error("Test failed"); }
static void put16(std::vector<uint8_t>& b, size_t o, uint16_t v) { b[o] = uint8_t(v); b[o+1] = uint8_t(v >> 8); }
static void put32(std::vector<uint8_t>& b, size_t o, uint32_t v) { for (int j=0;j<4;++j) b[o+j]=uint8_t(v >> (8*j)); }
static std::vector<uint8_t> fixture(bool pe64) {
    std::vector<uint8_t> b(1024);
    put16(b,0,0x5a4d); put32(b,60,64); put32(b,64,0x4550);
    put16(b,70,2); put16(b,84,64); put16(b,88,pe64 ? 0x20b : 0x10b); put32(b,148,512);
    std::memcpy(b.data()+152,".data",5); put32(b,168,256); put32(b,172,512);
    std::memcpy(b.data()+192,".bss",4);
    for (size_t i=0;i<256;++i) b[512+i]=uint8_t(i);
    return b;
}
int main() {
    try {
        std::vector<uint8_t> zeros(4096); check(entropy::shannon(zeros.data(), zeros.size()) == 0);
        uint8_t pair[] = {0,255}; check(std::abs(entropy::shannon(pair,2)-1) < 1e-12);
        std::vector<uint8_t> mapped(256 * 256);
        for (size_t i=0;i<mapped.size();++i) mapped[i]=uint8_t(i%256);
        auto blocks=entropy::map(mapped.data(),mapped.size());
        for (double value:blocks) check(std::abs(value-8)<1e-12);
        std::fill(mapped.begin(),mapped.begin()+256,uint8_t{0});
        blocks=entropy::map(mapped.data(),mapped.size());
        check(blocks[0]==0 && std::abs(blocks[1]-8)<1e-12);
        mapped.assign(513,0); mapped.back()=255;
        blocks=entropy::map(mapped.data(),mapped.size());
        check(blocks[254]==0 && std::abs(blocks[255]-entropy::shannon(mapped.data()+510,3))<1e-12);
        blocks=entropy::map(pair,2);
        int occupied=0; for (double value:blocks) if (value>=0) {++occupied; check(value==0);} check(occupied==2);
        for (size_t count : {16,32,64,128,256,512,1024}) {
            mapped.resize(count * 256);
            for(size_t i=0;i<mapped.size();++i) mapped[i]=uint8_t(i%256);
            blocks=entropy::map(mapped.data(),mapped.size(),count);
            check(blocks.size()==count);
            for(double value:blocks) check(std::abs(value-8)<1e-12);
        }
        for (bool pe64 : {false,true}) {
            auto b=fixture(pe64); auto r=entropy::analyze(b.data(),b.size());
            check(r.rows.size()==3 && !r.truncated);
            check(std::abs(r.rows[0].value-8)<1e-12);
            check(r.rows[1].available==0);
            check(r.rows[2].offset==768 && r.rows[2].available==256 && r.rows[2].value==0);
            b.resize(768); check(entropy::analyze(b.data(),b.size()).rows.size()==2);
            b.resize(600); r=entropy::analyze(b.data(),b.size()); check(r.truncated && r.rows[0].available==88);
            put32(b,172,0xfffffff0); r=entropy::analyze(b.data(),b.size()); check(r.truncated && r.rows[0].available==0 && r.rows.size()==2);
            b=fixture(pe64); put16(b,70,65535);
            bool failed=false; try { entropy::analyze(b.data(),b.size()); } catch (...) {failed=true;} check(failed);
            b=fixture(pe64); put32(b,60,0xfffffff0);
            failed=false; try { entropy::analyze(b.data(),b.size()); } catch (...) {failed=true;} check(failed);
        }
        // Out-of-order sections: overlay starts after the greatest raw end.
        auto b=fixture(false); put32(b,168,128); put32(b,172,768); put32(b,208,128); put32(b,212,512);
        auto r=entropy::analyze(b.data(),b.size()); check(r.rows.back().offset==896);
        for (size_t n=0;n<232;++n) { bool failed=false; try {entropy::analyze(b.data(),n);}catch(...){failed=true;} check(failed); }
        std::ifstream file("C:/Program Files/NTCore/Explorer Suite/CFF Explorer.exe",std::ios::binary);
        std::vector<uint8_t> real((std::istreambuf_iterator<char>(file)),{});
        r=entropy::analyze(real.data(),real.size()); check(!r.rows.empty() && !r.truncated);
        for (const auto& row:r.rows) std::cout << row.name << " offset=" << row.offset << " size=" << row.available << " H=" << row.value << '\n';
        std::cout << "All entropy and PE parsing tests passed.\n";
    } catch (const std::exception& e) {std::cerr << e.what() << '\n'; return 1;}
}

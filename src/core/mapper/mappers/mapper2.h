#pragma once
#include "../mapper.h"

// Mapper 2 — UxROM (switch 16K PRG lower, fix upper)
class Mapper2 : public Mapper {
public:
    using Mapper::Mapper;
    u8 cpuRead(u16 addr) override {
        if (addr < 0x8000) return 0;
        int numBanks = (int)rom_.prgROM.size() / 0x4000;
        if (addr >= 0xC000) return rom_.prgROM[(numBanks-1)*0x4000 + (addr-0xC000)];
        return rom_.prgROM[prgBank_ * 0x4000 + (addr-0x8000)];
    }
    void cpuWrite(u16 addr, u8 val) override {
        if (addr >= 0x8000) prgBank_ = val & 0x0F;
    }
    u8 ppuRead(u16 addr) override {
        addr &= 0x1FFF;
        if (!rom_.chrROM.empty()) return rom_.chrROM[addr];
        return rom_.chrRAM[addr];
    }
    void ppuWrite(u16 addr, u8 val) override {
        if (rom_.hasCHR_RAM()) rom_.chrRAM[addr&0x1FFF] = val;
    }
private:
    int prgBank_{0};
};

// Mapper 3 — CNROM (switch 8K CHR)
class Mapper3 : public Mapper {
public:
    using Mapper::Mapper;
    u8 cpuRead(u16 addr) override {
        if (addr < 0x8000) return 0;
        u16 off = addr - 0x8000;
        if (rom_.prgBanks == 1) off &= 0x3FFF;
        return rom_.prgROM[off % rom_.prgROM.size()];
    }
    void cpuWrite(u16 addr, u8 val) override {
        if (addr >= 0x8000) chrBank_ = val & 3;
    }
    u8 ppuRead(u16 addr) override {
        addr &= 0x1FFF;
        return rom_.chrROM[(chrBank_*0x2000 + addr) % rom_.chrROM.size()];
    }
    void ppuWrite(u16 addr, u8 val) override { (void)addr; (void)val; }
private:
    int chrBank_{0};
};

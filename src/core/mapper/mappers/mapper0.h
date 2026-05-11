#pragma once
#include "../mapper.h"

// ============================================================
// Mapper 0 — NROM (no banking)
// ============================================================
class Mapper0 : public Mapper {
public:
    using Mapper::Mapper;

    u8 cpuRead(u16 addr) override {
        if (addr < 0x8000) return 0;
        // Mirror: 16K NROM-128 or 32K NROM-256
        u16 offset = addr - 0x8000;
        if (rom_.prgBanks == 1) offset &= 0x3FFF;
        return rom_.prgROM[offset % rom_.prgROM.size()];
    }
    void cpuWrite(u16 addr, u8) override { (void)addr; }

    u8 ppuRead(u16 addr) override {
        addr &= 0x1FFF;
        if (!rom_.chrROM.empty()) return rom_.chrROM[addr];
        return rom_.chrRAM[addr];
    }
    void ppuWrite(u16 addr, u8 val) override {
        if (rom_.hasCHR_RAM()) rom_.chrRAM[addr & 0x1FFF] = val;
    }
};

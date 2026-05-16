#pragma once
#include "../types.h"
#include "../cpu/cpu6502.h"
#include "../ppu/ppu.h"
#include "../apu/apu.h"
#include "../mapper/mapper.h"
#include <array>
#include <memory>
#include <functional>

// ============================================================
// NES Memory Bus — implements IBus, glues CPU/PPU/APU/Mapper
// ============================================================
class NESBus : public IBus {
public:
    // System RAM: 2KB mirrored to 8KB ($0000-$1FFF)
    u8 ram[2048]{};

    // Controllers
    u8 controllerState[2]{0, 0};   // Current button state (set by input layer)
    u8 controllerShift[2]{0, 0};   // Shift register for serial reads
    bool controllerStrobe{false};

    // VS System coin input (for arcade games like VS Battle City)
    bool vsCoinPressed{false};

    // Components (owned by NESConsole, referenced here)
    PPU*     ppu{nullptr};
    APU*     apu{nullptr};
    Mapper*  mapper{nullptr};

    // CPU bus read
    u8 read(u16 addr) override {
        if (addr < 0x2000) return ram[addr & 0x7FF];
        if (addr < 0x4000) return ppu ? ppu->regRead(addr) : 0;
        if (addr == 0x4015) return apu ? apu->regRead(addr) : 0;
        if (addr == 0x4016) {
            u8 v;
            if (controllerStrobe) {
                v = 0x40 | ((controllerState[0] >> 7) & 1);
            } else {
                v = 0x40 | ((controllerShift[0] >> 7) & 1);
                controllerShift[0] <<= 1;
            }
            // VS System: coin input on bit 4 (0x10) when reading $4016
            if (vsCoinPressed) v |= 0x10;
            return v;
        }
        if (addr == 0x4017) {
            if (controllerStrobe) return 0x40 | ((controllerState[1] >> 7) & 1);
            u8 v = 0x40 | ((controllerShift[1] >> 7) & 1);
            controllerShift[1] <<= 1;
            return v;
        }
        if (addr >= 0x4020 && addr < 0x6000) return 0; // expansion
        if (addr >= 0x6000 && addr < 0x8000) return mapper ? mapper->sramRead(addr) : 0;
        if (addr >= 0x8000) return mapper ? mapper->cpuRead(addr) : 0;
        return 0;
    }

    // CPU bus write
    void write(u16 addr, u8 val) override {
        if (addr < 0x2000) { ram[addr & 0x7FF] = val; return; }
        if (addr < 0x4000) { if(ppu) ppu->regWrite(addr, val); return; }
        if (addr == 0x4014) {
            // OAM DMA: copy 256 bytes from $XX00-$XXFF
            u16 base = (u16)val << 8;
            u8 page[256];
            for (int i = 0; i < 256; i++) page[i] = read(base + i);
            if (ppu) ppu->oamDMA(page);
            // OAM DMA stalls CPU: handled in NESConsole::runFrame
            dmaCycles_ = 513; // will be consumed
            return;
        }
        if (addr == 0x4016) {
            bool prev = controllerStrobe;
            controllerStrobe = val & 1;
            if (prev && !controllerStrobe) {
                // Latch controller states
                controllerShift[0] = controllerState[0];
                controllerShift[1] = controllerState[1];
            }
            return;
        }
        if (addr >= 0x4000 && addr < 0x4018) { if(apu) apu->regWrite(addr, val); return; }
        if (addr >= 0x6000 && addr < 0x8000) { if(mapper) mapper->sramWrite(addr, val); return; }
        if (addr >= 0x8000) { if(mapper) mapper->cpuWrite(addr, val); return; }
    }

    // DMA stall cycles pending
    int dmaCycles_{0};
};

// ============================================================
// PPU bus (CHR + nametable)
// ============================================================
class PPUBus {
public:
    Mapper* mapper{nullptr};
    PPU*    ppu{nullptr};

    u8 read(u16 addr) {
        addr &= 0x3FFF;
        if (addr < 0x2000) return mapper ? mapper->ppuRead(addr) : 0;
        if (addr < 0x3F00) {
            // Nametable
            u16 mirrored = mapper ? mapper->mirrorAddress(addr) : (addr & 0x0FFF);
            return ppu ? ppu->nameTableRAM[mirrored & 0x07FF] : 0;
        }
        // Palette handled directly by PPU
        return 0;
    }

    void write(u16 addr, u8 val) {
        addr &= 0x3FFF;
        if (addr < 0x2000) { if(mapper) mapper->ppuWrite(addr, val); return; }
        if (addr < 0x3F00) {
            u16 mirrored = mapper ? mapper->mirrorAddress(addr) : (addr & 0x0FFF);
            if (ppu) ppu->nameTableRAM[mirrored & 0x07FF] = val;
        }
    }
};

#pragma once
#include "../mapper.h"

// ============================================================
// Mapper 1 — MMC1 (SxROM)
// ============================================================
class Mapper1 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() {
        shiftReg_ = 0x10; writeCount_ = 0;
        prgMode_ = 3; chrMode_ = 0;
        prgBank_ = 0; chrBank0_ = 0; chrBank1_ = 0;
        // mirror will use rom default
    }

    u8 cpuRead(u16 addr) override {
        if (addr >= 0x6000 && addr < 0x8000) return sramRead(addr);
        if (addr < 0x8000) return 0;
        int offset = addr - 0x8000;
        int numBanks = (int)rom_.prgROM.size() / 0x4000;
        if (prgMode_ == 0 || prgMode_ == 1) {
            // 32KB mode
            int bank = (prgBank_ & ~1);
            return rom_.prgROM[bank * 0x4000 + (offset % 0x8000)];
        } else if (prgMode_ == 2) {
            // fix first bank at 0, switch second
            if (addr < 0xC000) return rom_.prgROM[offset];
            return rom_.prgROM[prgBank_ * 0x4000 + (addr - 0xC000)];
        } else {
            // fix last bank, switch first
            if (addr >= 0xC000) return rom_.prgROM[(numBanks-1)*0x4000 + (addr-0xC000)];
            return rom_.prgROM[prgBank_ * 0x4000 + offset];
        }
    }

    void cpuWrite(u16 addr, u8 val) override {
        if (addr >= 0x6000 && addr < 0x8000) { sramWrite(addr, val); return; }
        if (addr < 0x8000) return;

        if (val & 0x80) { shiftReg_ = 0x10; writeCount_ = 0; prgMode_ = 3; return; }

        shiftReg_ = ((val & 1) << 4) | (shiftReg_ >> 1);
        writeCount_++;
        if (writeCount_ == 5) {
            int reg = (addr >> 13) & 3;
            switch (reg) {
            case 0: // Control
                switch (shiftReg_ & 3) {
                case 0: rom_.mirror = MirrorType::SingleLow;  break;
                case 1: rom_.mirror = MirrorType::SingleHigh; break;
                case 2: rom_.mirror = MirrorType::Vertical;   break;
                case 3: rom_.mirror = MirrorType::Horizontal; break;
                }
                prgMode_ = (shiftReg_ >> 2) & 3;
                chrMode_ = (shiftReg_ >> 4) & 1;
                break;
            case 1: chrBank0_ = shiftReg_ & 0x1F; break;
            case 2: chrBank1_ = shiftReg_ & 0x1F; break;
            case 3:
                prgBank_ = shiftReg_ & 0x0F;
                break;
            }
            shiftReg_ = 0x10; writeCount_ = 0;
        }
    }

    u8 ppuRead(u16 addr) override {
        addr &= 0x1FFF;
        auto& chr = rom_.hasCHR_RAM() ? rom_.chrRAM : rom_.chrROM;
        if (chr.empty()) return 0;
        if (chrMode_ == 0) {
            // 8KB mode
            int bank = (chrBank0_ & ~1);
            return chr[(bank * 0x1000 + addr) % chr.size()];
        } else {
            // 4KB mode
            if (addr < 0x1000) return chr[(chrBank0_ * 0x1000 + addr) % chr.size()];
            else return chr[(chrBank1_ * 0x1000 + (addr-0x1000)) % chr.size()];
        }
    }
    void ppuWrite(u16 addr, u8 val) override {
        if (rom_.hasCHR_RAM()) rom_.chrRAM[addr & 0x1FFF] = val;
    }

private:
    u8  shiftReg_{0x10}, writeCount_{0};
    int prgMode_{3}, chrMode_{0};
    int prgBank_{0}, chrBank0_{0}, chrBank1_{0};
};

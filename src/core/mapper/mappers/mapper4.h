#pragma once
#include "../mapper.h"
#include <array>

// ============================================================
// Mapper 4 — MMC3 (TxROM)
// ============================================================
class Mapper4 : public Mapper {
public:
    explicit Mapper4(NESRom& rom) : Mapper(rom) {
        // Default PRG registers
        prgRegs_[0] = 0;
        prgRegs_[1] = 1;
        // Default CHR
        for (auto& r : chrRegs_) r = 0;
        updateBanks();
    }

    u8 cpuRead(u16 addr) override {
        if (addr >= 0x6000 && addr < 0x8000) return sramRead(addr);
        if (addr < 0x8000) return 0;
        int off = addr - 0x8000;
        int bank = prgMap_[off / 0x2000];
        return rom_.prgROM[bank * 0x2000 + (off % 0x2000)];
    }

    void cpuWrite(u16 addr, u8 val) override {
        if (addr >= 0x6000 && addr < 0x8000) { sramWrite(addr, val); return; }
        if (addr < 0x8000) return;
        bool even = !(addr & 1);
        if (addr < 0xA000) {
            if (even) { bankSelect_ = val & 7; prgMode_ = (val>>6)&1; chrMode_ = (val>>7)&1; }
            else { regs_[bankSelect_] = val; updateBanks(); }
        } else if (addr < 0xC000) {
            if (even) rom_.mirror = (val&1) ? MirrorType::Horizontal : MirrorType::Vertical;
        } else if (addr < 0xE000) {
            if (even) irqLatch_ = val;
            else { irqCounter_ = 0; irqReload_ = true; }
        } else {
            if (even) irqEnabled_ = false;
            else irqEnabled_ = true;
        }
    }

    u8 ppuRead(u16 addr) override {
        addr &= 0x1FFF;
        int bank = chrMap_[addr / 0x400];
        auto& chr = rom_.hasCHR_RAM() ? rom_.chrRAM : rom_.chrROM;
        if (chr.empty()) return 0;
        return chr[(bank * 0x400 + (addr % 0x400)) % chr.size()];
    }

    void ppuWrite(u16 addr, u8 val) override {
        if (rom_.hasCHR_RAM()) rom_.chrRAM[addr & 0x1FFF] = val;
    }

    void scanlineIRQ() override {
        if (irqCounter_ == 0 || irqReload_) {
            irqCounter_ = irqLatch_;
            irqReload_ = false;
        } else irqCounter_--;
        if (irqCounter_ == 0 && irqEnabled_ && irqCB_) irqCB_();
    }

private:
    u8  regs_[8]{};
    u8  bankSelect_{0};
    int prgMode_{0}, chrMode_{0};
    int prgMap_[4]{};
    int chrMap_[8]{};
    u8  prgRegs_[2]{};
    u8  chrRegs_[6]{};

    u8  irqLatch_{0}, irqCounter_{0};
    bool irqEnabled_{false}, irqReload_{false};

    void updateBanks() {
        int numPRG = (int)rom_.prgROM.size() / 0x2000;
        int lastBank = numPRG - 1;
        if (prgMode_ == 0) {
            prgMap_[0] = regs_[6] & (numPRG-1);
            prgMap_[1] = regs_[7] & (numPRG-1);
            prgMap_[2] = (lastBank-1) & (numPRG-1);
            prgMap_[3] = lastBank;
        } else {
            prgMap_[0] = (lastBank-1) & (numPRG-1);
            prgMap_[1] = regs_[7] & (numPRG-1);
            prgMap_[2] = regs_[6] & (numPRG-1);
            prgMap_[3] = lastBank;
        }

        int numCHR = (int)(rom_.hasCHR_RAM() ? rom_.chrRAM.size() : rom_.chrROM.size()) / 0x400;
        if (numCHR == 0) numCHR = 1;
        if (chrMode_ == 0) {
            chrMap_[0] = (regs_[0]&~1)   % numCHR;
            chrMap_[1] = (regs_[0]|1)    % numCHR;
            chrMap_[2] = (regs_[1]&~1)   % numCHR;
            chrMap_[3] = (regs_[1]|1)    % numCHR;
            chrMap_[4] = regs_[2]         % numCHR;
            chrMap_[5] = regs_[3]         % numCHR;
            chrMap_[6] = regs_[4]         % numCHR;
            chrMap_[7] = regs_[5]         % numCHR;
        } else {
            chrMap_[0] = regs_[2]         % numCHR;
            chrMap_[1] = regs_[3]         % numCHR;
            chrMap_[2] = regs_[4]         % numCHR;
            chrMap_[3] = regs_[5]         % numCHR;
            chrMap_[4] = (regs_[0]&~1)   % numCHR;
            chrMap_[5] = (regs_[0]|1)    % numCHR;
            chrMap_[6] = (regs_[1]&~1)   % numCHR;
            chrMap_[7] = (regs_[1]|1)    % numCHR;
        }
    }
};

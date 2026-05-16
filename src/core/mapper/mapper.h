#pragma once
#include "../types.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>

// ============================================================
// iNES ROM Format Parser
// ============================================================

enum class MirrorType { Horizontal, Vertical, FourScreen, SingleLow, SingleHigh };

struct NESRom {
    std::vector<u8> prgROM;   // PRG-ROM data
    std::vector<u8> chrROM;   // CHR-ROM data (empty = CHR-RAM)
    std::vector<u8> chrRAM;   // Internal CHR-RAM (if no CHR-ROM)

    int  mapperNum{0};
    MirrorType mirror{MirrorType::Horizontal};
    bool hasBattery{false};
    bool hasTrainer{false};
    int  prgBanks{0};         // number of 16KB PRG banks
    int  chrBanks{0};         // number of 8KB CHR banks

    // Raw iNES header (16 bytes) for extended flags
    u8 header[16]{};

    // Load from file; returns true on success
    static bool load(const std::string& path, NESRom& rom);

    bool hasCHR_RAM() const { return chrBanks == 0; }
    int  prgSize() const    { return (int)prgROM.size(); }
    int  chrSize() const    { return (int)chrROM.size(); }
};

// ============================================================
// Mapper Base Class
// ============================================================
class NESConsole;
class Mapper {
    friend class NESConsole;
public:
    explicit Mapper(NESRom& rom) : rom_(rom) {}
    virtual ~Mapper() = default;

    // CPU bus ($8000-$FFFF)
    virtual u8   cpuRead(u16 addr)          = 0;
    virtual void cpuWrite(u16 addr, u8 val) = 0;

    // CPU bus ($6000-$7FFF) PRG-RAM (SRAM)
    virtual u8   sramRead(u16 addr)          { return sram_[addr & 0x1FFF]; }
    virtual void sramWrite(u16 addr, u8 val) { sram_[addr & 0x1FFF] = val; }

    // PPU bus ($0000-$1FFF) CHR
    virtual u8   ppuRead(u16 addr)          = 0;
    virtual void ppuWrite(u16 addr, u8 val) { if(rom_.hasCHR_RAM()) rom_.chrRAM[addr&0x1FFF]=val; }

    // Nametable mirroring
    virtual u16  mirrorAddress(u16 addr) const;

    // Called at end of each scanline (for MMC3-style IRQ)
    virtual void scanlineIRQ() {}

    // IRQ callback
    void setIRQCallback(std::function<void()> cb) { irqCB_ = std::move(cb); }

    static std::unique_ptr<Mapper> create(NESRom& rom);

protected:
    NESRom& rom_;
    u8 sram_[8192]{};
    std::function<void()> irqCB_;

    u8* prgBank(int bank16k, int offset = 0) const {
        int b = bank16k * 0x4000 + offset;
        b %= (int)rom_.prgROM.size();
        return const_cast<u8*>(&rom_.prgROM[b]);
    }
    u8* chrBank(int bank8k, int offset = 0) const {
        auto& chr = rom_.hasCHR_RAM() ? rom_.chrRAM : rom_.chrROM;
        int b = bank8k * 0x2000 + offset;
        if (b >= (int)chr.size()) b %= (int)chr.size();
        return const_cast<u8*>(&chr[b]);
    }
    u8* chrBank1k(int bank, int offset = 0) const {
        auto& chr = rom_.hasCHR_RAM() ? rom_.chrRAM : rom_.chrROM;
        int b = bank * 0x0400 + offset;
        if (!chr.empty()) b %= (int)chr.size();
        return const_cast<u8*>(&chr[b]);
    }
};

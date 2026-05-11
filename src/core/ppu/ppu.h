#pragma once
#include "../types.h"
#include <functional>
#include <array>

// Forward declarations
class IBus;

// ============================================================
// PPU (Picture Processing Unit) — NES 2C02
// ============================================================
class NESConsole;
class PPU {
    friend class NESConsole;
public:
    // 256x240 ARGB8888 framebuffer (written after each frame)
    u32 framebuffer[NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT]{};
    bool frameReady{false};

    // Connect callbacks
    void setNMICallback(std::function<void()> cb) { nmiCallback_ = std::move(cb); }
    void setReadCallback(std::function<u8(u16)> cb)    { busRead_  = std::move(cb); }
    void setWriteCallback(std::function<void(u16,u8)> cb){ busWrite_ = std::move(cb); }

    void reset();
    void powerOn();

    // CPU-side register access (mapped at $2000-$2007)
    u8   regRead(u16 addr);
    void regWrite(u16 addr, u8 val);

    // OAM DMA (CPU writes 256 bytes starting at $4014)
    void oamDMA(const u8* data);

    // Run N PPU clock cycles; returns number of CPU cycles of pending NMI
    void tick(int ppuCycles);

    // Palette RAM (internal) — 32 bytes
    u8 paletteRAM[32]{};

    // OAM — 256 bytes
    u8 oam[256]{};

    // Nametable RAM — 2KB internal (mirroring handled by mapper)
    u8 nameTableRAM[2048]{};

    // Current scanline / dot (for mapper IRQs)
    int scanline{0}, dot{0};

    // Expose for mapper scanline IRQ
    bool renderingEnabled() const;

    // Debug accessors (read-only, no side effects)
    u8 debugCtrl()  const { return ctrl_; }
    u8 debugMask()  const { return mask_; }
    u8 debugStatus() const { return status_; }

private:
    // ---- Registers ----
    u8  ctrl_{0};    // PPUCTRL  $2000
    u8  mask_{0};    // PPUMASK  $2001
    u8  status_{0};  // PPUSTATUS $2002
    u8  oamAddr_{0}; // OAMADDR  $2003
    u8  fineX_{0};   // Fine X scroll (3 bits)
    u16 v_{0};       // Current VRAM address (15 bits)
    u16 t_{0};       // Temporary VRAM address
    bool wToggle_{false}; // Write toggle (w)

    // Read buffer for $2007
    u8 readBuffer_{0};

    // Internal frame / odd-frame flag
    bool oddFrame_{false};
    int  totalCycles_{0};

    // Sprite evaluation buffers
    u8   spriteCount_{0};
    u8   spriteBuf_[8][4]{};  // 8 sprites, 4 bytes each (Y,tile,attr,X)
    u8   spritePatLo_[8]{};
    u8   spritePatHi_[8]{};

    // Background shift registers
    u16 bgPatLo_{0}, bgPatHi_{0};
    u16 bgAttrLo_{0}, bgAttrHi_{0};
    u8  bgAttrLatch_{0};
    u8  bgNTLatch_{0};
    u8  bgAttrByteLatch_{0};
    // Fetched pattern data (latches for next load)
    u8  bgPatLoLatch_{0};
    u8  bgPatHiLatch_{0};

    // Callbacks
    mutable std::function<void()>       nmiCallback_;
    mutable std::function<u8(u16)>      busRead_;
    mutable std::function<void(u16,u8)> busWrite_;

    // ---- Internal helpers ----
    u8   ppuRead(u16 addr) const;
    void ppuWrite(u16 addr, u8 val);
    u32  getPaletteColor(u8 idx) const;

    void incrementVX();
    void incrementVY();
    void transferHorizV();
    void transferVertV();
    void loadBGShiftRegisters();
    void shiftBGRegisters();

    void fetchBackground();
    void evaluateSprites();
    void renderPixel();

    // Nametable / attribute fetch
    u8 fetchNT()   const;
    u8 fetchAT()   const;
    u8 fetchBGLo() const;
    u8 fetchBGHi() const;

    static const u32 kNESPalette[64];
};

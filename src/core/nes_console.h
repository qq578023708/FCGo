#pragma once
#include "cpu/cpu6502.h"
#include "ppu/ppu.h"
#include "apu/apu.h"
#include "mapper/mapper.h"
#include "memory/bus.h"
#include "debug/breakpoint.h"
#include "types.h"
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <cstring>

// ============================================================
// Controller button bits (standard NES joypad)
// ============================================================
enum class Button : u8 {
    A      = 0x80,
    B      = 0x40,
    Select = 0x20,
    Start  = 0x10,
    Up     = 0x08,
    Down   = 0x04,
    Left   = 0x02,
    Right  = 0x01
};

// ============================================================
// NESConsole — top-level emulator machine
// ============================================================
class NESConsole {
public:
    NESConsole();
    ~NESConsole() = default;

    // Load ROM; returns false on failure
    bool loadROM(const std::string& path);

    // Power on / reset
    void powerOn();
    void reset();

    // Run one full frame (advance CPU/PPU/APU until frame complete)
    void runFrame();

    // Access framebuffer (256x240 ARGB8888) — valid after runFrame()
    const u32* getFramebuffer() const { return ppu_.framebuffer; }

    // Access audio samples — valid after runFrame()
    void getAudioSamples(std::vector<float>& out) { apu_.getSamples(out); }

    // Controller input
    void setController(int player, u8 buttons) {
        bus_.controllerState[player & 1] = buttons;
    }
    u8 getControllerState(int player) const {
        return bus_.controllerState[player & 1];
    }
    void pressButton(int player, Button b) {
        bus_.controllerState[player&1] |= (u8)b;
    }
    void releaseButton(int player, Button b) {
        bus_.controllerState[player&1] &= ~(u8)b;
    }

    // Mapper IRQ trigger hook (external use)
    void setIRQCallback(std::function<void()> cb) { mapper_->setIRQCallback(std::move(cb)); }

    // Debug
    CPU6502& getCPU()  { return cpu_; }
    PPU&     getPPU()  { return ppu_; }
    APU&     getAPU()  { return apu_; }
    NESBus&  getBus()  { return bus_; }
    BreakpointManager& getBreakpoints() { return breakpoints_; }

    // Memory breakpoint hit during runFrame (write/read)
    bool hasMemBreakpointHit() const { return memBPHitAddr_ >= 0; }
    uint16_t getMemBPHitAddr() const { return static_cast<uint16_t>(memBPHitAddr_); }
    void clearMemBPHit() { memBPHitAddr_ = -1; }

    bool isLoaded() const { return romLoaded_; }
    const std::string& romPath() const { return romPath_; }

    // State save/load (for netplay sync)
    std::vector<u8> saveState() const;
    void loadState(const std::vector<u8>& data);

private:
    CPU6502  cpu_;
    PPU      ppu_;
    APU      apu_;
    NESBus   bus_;
    PPUBus   ppuBus_;
    BreakpointManager breakpoints_;
    int memBPHitAddr_ = -1;

    NESRom   rom_;
    std::unique_ptr<Mapper> mapper_;
    bool romLoaded_{false};
    std::string romPath_;

    // PPU cycle budget (3 PPU ticks per CPU tick)
    void tickCPU();
};

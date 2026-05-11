#pragma once

#include "types.h"
#include <vector>
#include <functional>
#include <cstdint>
#include <string>

// ============================================================
// Breakpoint flags (FCEUX-compatible)
// ============================================================
#define BP_ENABLE   0x01  // Breakpoint enabled
#define BP_READ     0x02  // Read breakpoint (watchpoint)
#define BP_WRITE    0x04  // Write breakpoint (watchpoint)
#define BP_EXEC     0x08  // Execute breakpoint

// Memory region flags
#define BP_CPU      0x00  // CPU memory (default)
#define BP_PPU      0x20  // PPU memory
#define BP_OAM      0x40  // Sprite OAM memory
#define BP_ROM      0x80  // ROM memory

// ============================================================
// Breakpoint data structure
// ============================================================
struct Breakpoint {
    uint16_t address = 0;       // Start address
    uint16_t endAddress = 0;    // End address (0 = single address)
    uint16_t flags = 0;         // BP_ENABLE | BP_READ/WRITE/EXEC | BP_CPU/PPU/OAM/ROM
    std::string desc;           // Description
    int hitCount = 0;           // Number of times hit

    bool isEnabled() const { return flags & BP_ENABLE; }
    bool isRead() const { return flags & BP_READ; }
    bool isWrite() const { return flags & BP_WRITE; }
    bool isExec() const { return flags & BP_EXEC; }
    bool matchesAddr(uint16_t addr) const {
        if (endAddress == 0) return address == addr;
        return addr >= address && addr <= endAddress;
    }
};

// ============================================================
// Breakpoint manager — shared across debugger, memory viewer
// ============================================================
class BreakpointManager {
public:
    // Max breakpoints (FCEUX uses 64)
    static constexpr int kMaxBreakpoints = 64;

    // Add/remove
    int add(const Breakpoint& bp);
    bool remove(int index);
    void clear();
    int count() const { return static_cast<int>(breakpoints_.size()); }
    const Breakpoint& at(int index) const { return breakpoints_[index]; }
    Breakpoint& at(int index) { return breakpoints_[index]; }

    // Toggle breakpoint at address with given type flags
    // Returns index of the breakpoint, or -1
    int toggle(uint16_t addr, uint16_t typeFlags);

    // Check if any breakpoint matches
    // Returns true if a breakpoint was hit, fills out hitInfo
    bool checkExec(uint16_t pc, int& hitIndex);
    bool checkRead(uint16_t addr, int& hitIndex);
    bool checkWrite(uint16_t addr, int& hitIndex);

    // Callback when breakpoint is hit: void(int bpIndex, uint16_t addr, uint16_t type)
    using HitCallback = std::function<void(int, uint16_t, uint16_t)>;
    void setHitCallback(HitCallback cb) { hitCallback_ = std::move(cb); }

    // Enable/disable all
    void setAllEnabled(bool enabled);
    bool isEmpty() const { return breakpoints_.empty(); }

private:
    std::vector<Breakpoint> breakpoints_;
    HitCallback hitCallback_;

    bool checkMatch(uint16_t addr, uint16_t accessType, int& hitIndex);
};

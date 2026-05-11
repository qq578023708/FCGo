#pragma once
#include "types.h"
#include <functional>
#include <array>

class BreakpointManager;

// ============================================================
// NES Memory Bus Interface
// ============================================================
class IBus {
public:
    virtual ~IBus() = default;
    virtual u8  read(u16 addr) = 0;
    virtual void write(u16 addr, u8 val) = 0;
};

// ============================================================
// CPU (MOS 6502) — cycle-accurate NTSC NES variant
// ============================================================
class CPU6502 {
public:
    // Registers
    u8  A{0}, X{0}, Y{0}, SP{0xFD}, P{0x24};
    u16 PC{0};

    // Cycle counter
    u64 cycles{0};
    // Pending interrupt
    Interrupt pendingIRQ{Interrupt::None};
    bool nmiPending{false};

    void connect(IBus* bus);
    void setBreakpointManager(BreakpointManager* bpm) { bpm_ = bpm; }
    void reset();
    void nmi();
    void irq();
    // Execute one instruction, returns cycles consumed
    int  step();

    // Peek at current PC without side effects (for debug)
    u16  getPC() const { return PC; }
    bool getMemBPHit() const { return memBreakpointHit_; }

private:
    IBus* bus_{nullptr};
    BreakpointManager* bpm_{nullptr};
    bool memBreakpointHit_ = false;

    // Internal read/write with cycle tracking
    u8   read(u16 addr);
    void write(u16 addr, u8 val);
    u8   readPC();
    u16  readPC16();
    u8   peek(u16 addr) const;

    void push(u8 val);
    u8   pop();
    void push16(u16 val);
    u16  pop16();

    void setZN(u8 val);
    bool pageCrossed(u16 a, u16 b) const { return (a & 0xFF00) != (b & 0xFF00); }

    // Addressing modes — return effective address + extra cycle flag
    u16  addrImmediate(bool& extra);
    u16  addrZeroPage(bool& extra);
    u16  addrZeroPageX(bool& extra);
    u16  addrZeroPageY(bool& extra);
    u16  addrAbsolute(bool& extra);
    u16  addrAbsoluteX(bool& extra);
    u16  addrAbsoluteY(bool& extra);
    u16  addrIndirectX(bool& extra);
    u16  addrIndirectY(bool& extra);

    // Branch helper
    int branch(bool cond);

    // Extra cycles accumulator per instruction
    int extraCycles_{0};
};

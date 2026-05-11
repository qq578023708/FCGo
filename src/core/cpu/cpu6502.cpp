#include "cpu6502.h"
#include "../debug/breakpoint.h"
#include <cassert>
#include <stdexcept>

// ============================================================
// Cycle counts per opcode (official + undocumented)
// ============================================================
static const u8 kCycles[256] = {
    7,6,2,8,3,3,5,5,3,2,2,2,4,4,6,6, // 0x
    2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7, // 1x
    6,6,2,8,3,3,5,5,4,2,2,2,4,4,6,6, // 2x
    2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7, // 3x
    6,6,2,8,3,3,5,5,3,2,2,2,3,4,6,6, // 4x
    2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7, // 5x
    6,6,2,8,3,3,5,5,4,2,2,2,5,4,6,6, // 6x
    2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7, // 7x
    2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4, // 8x
    2,6,2,6,4,4,4,4,2,5,2,5,5,5,5,5, // 9x
    2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4, // Ax
    2,5,2,5,4,4,4,4,2,4,2,4,4,4,4,4, // Bx
    2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6, // Cx
    2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7, // Dx
    2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6, // Ex
    2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7  // Fx
};

// ============================================================
void CPU6502::connect(IBus* bus) { bus_ = bus; }

void CPU6502::reset() {
    A = X = Y = 0;
    SP = 0xFD;
    P  = 0x24;
    u8 lo = bus_->read(0xFFFC);
    u8 hi = bus_->read(0xFFFD);
    PC = (u16)(lo | (hi << 8));
    cycles = 0;
    pendingIRQ = Interrupt::None;
    nmiPending = false;
}

void CPU6502::nmi()         { nmiPending = true; }
void CPU6502::irq()         { if (!(P & CPU_FLAG_I)) pendingIRQ = Interrupt::IRQ; }

// ---- private helpers ----
u8 CPU6502::read(u16 addr) {
    u8 val = bus_->read(addr);
    if (bpm_) {
        int hitIdx = -1;
        bpm_->checkRead(addr, hitIdx);
    }
    return val;
}
void CPU6502::write(u16 addr, u8 v) {
    if (bpm_) {
        int hitIdx = -1;
        if (bpm_->checkWrite(addr, hitIdx)) {
            if (bpm_->at(hitIdx).isEnabled()) {
                memBreakpointHit_ = true;
                return;  // Block the write, pause here
            }
        }
    }
    bus_->write(addr, v);
}
u8   CPU6502::readPC()               { return bus_->read(PC++); }
u16  CPU6502::readPC16()             { u8 lo=readPC(); u8 hi=readPC(); return lo|(hi<<8); }
u8   CPU6502::peek(u16 addr) const   { return const_cast<IBus*>(bus_)->read(addr); }

void CPU6502::push(u8 v)  { bus_->write(0x0100|SP--, v); }
u8   CPU6502::pop()       { return bus_->read(0x0100|(++SP)); }
void CPU6502::push16(u16 v){ push((v>>8)&0xFF); push(v&0xFF); }
u16  CPU6502::pop16()      { u8 lo=pop(); u8 hi=pop(); return lo|(hi<<8); }

void CPU6502::setZN(u8 v) {
    P = (P & ~(CPU_FLAG_Z|CPU_FLAG_N))
      | (v==0 ? CPU_FLAG_Z : 0)
      | (v&0x80 ? CPU_FLAG_N : 0);
}

// ---- addressing modes ----
u16 CPU6502::addrImmediate(bool& e)  { e=false; return PC++; }
u16 CPU6502::addrZeroPage(bool& e)   { e=false; return readPC(); }
u16 CPU6502::addrZeroPageX(bool& e)  { e=false; return (readPC()+X)&0xFF; }
u16 CPU6502::addrZeroPageY(bool& e)  { e=false; return (readPC()+Y)&0xFF; }
u16 CPU6502::addrAbsolute(bool& e)   { e=false; return readPC16(); }
u16 CPU6502::addrAbsoluteX(bool& e)  {
    u16 base=readPC16(); u16 ea=base+X;
    e = pageCrossed(base,ea); return ea;
}
u16 CPU6502::addrAbsoluteY(bool& e)  {
    u16 base=readPC16(); u16 ea=base+Y;
    e = pageCrossed(base,ea); return ea;
}
u16 CPU6502::addrIndirectX(bool& e)  {
    e=false;
    u8 ptr=(readPC()+X)&0xFF;
    u8 lo=read(ptr); u8 hi=read((ptr+1)&0xFF);
    return lo|(hi<<8);
}
u16 CPU6502::addrIndirectY(bool& e)  {
    u8 ptr=readPC();
    u8 lo=read(ptr); u8 hi=read((ptr+1)&0xFF);
    u16 base=lo|(hi<<8); u16 ea=base+Y;
    e = pageCrossed(base,ea); return ea;
}

int CPU6502::branch(bool cond) {
    i8 offset = (i8)readPC();
    if(!cond) return 0;
    u16 old = PC;
    PC = (u16)(PC + offset);
    return pageCrossed(old, PC) ? 2 : 1;
}

// ============================================================
// Main step — returns number of CPU cycles consumed
// ============================================================
int CPU6502::step() {
    // Handle NMI (highest priority)
    if (nmiPending) {
        nmiPending = false;
        push16(PC);
        push(P & ~CPU_FLAG_B);
        P |= CPU_FLAG_I;
        u8 lo = read(0xFFFA); u8 hi = read(0xFFFB);
        PC = lo | (hi<<8);
        cycles += 7;
        return 7;
    }
    // Handle IRQ
    if (pendingIRQ == Interrupt::IRQ && !(P & CPU_FLAG_I)) {
        pendingIRQ = Interrupt::None;
        push16(PC);
        push(P & ~CPU_FLAG_B);
        P |= CPU_FLAG_I;
        u8 lo = read(0xFFFE); u8 hi = read(0xFFFF);
        PC = lo | (hi<<8);
        cycles += 7;
        return 7;
    }

    u8 opcode = readPC();
    int cyc = kCycles[opcode];
    bool extra = false;
    u16 ea = 0;

    // Check execution breakpoint
    if (bpm_) {
        int hitIdx = -1;
        if (bpm_->checkExec(PC - 1, hitIdx)) {  // PC already incremented by readPC
            if (bpm_->at(hitIdx).isEnabled()) {
                // Breakpoint hit — callback will be invoked by the hit callback
                return cyc;
            }
        }
    }

#define ADDR_IMM    ea=addrImmediate(extra)
#define ADDR_ZP     ea=addrZeroPage(extra)
#define ADDR_ZPX    ea=addrZeroPageX(extra)
#define ADDR_ZPY    ea=addrZeroPageY(extra)
#define ADDR_ABS    ea=addrAbsolute(extra)
#define ADDR_ABSX   ea=addrAbsoluteX(extra)
#define ADDR_ABSY   ea=addrAbsoluteY(extra)
#define ADDR_INDX   ea=addrIndirectX(extra)
#define ADDR_INDY   ea=addrIndirectY(extra)
#define RD          read(ea)

    // Helper lambdas
    auto ADC = [&](u8 m){
        u16 r = A + m + (P&CPU_FLAG_C?1:0);
        P &= ~(CPU_FLAG_C|CPU_FLAG_Z|CPU_FLAG_V|CPU_FLAG_N);
        if(r>0xFF) P|=CPU_FLAG_C;
        if(!((u8)r)) P|=CPU_FLAG_Z;
        if((~(A^m))&(A^r)&0x80) P|=CPU_FLAG_V;
        if(r&0x80) P|=CPU_FLAG_N;
        A=(u8)r;
    };
    auto SBC = [&](u8 m){ ADC(m^0xFF); };
    auto CMP = [&](u8 reg, u8 m){
        u16 r=reg-m; setZN((u8)r);
        P=(r<0x100)?(P|CPU_FLAG_C):(P&~CPU_FLAG_C);
    };
    auto AND = [&](u8 m){ A&=m; setZN(A); };
    auto ORA = [&](u8 m){ A|=m; setZN(A); };
    auto EOR = [&](u8 m){ A^=m; setZN(A); };
    auto ASL = [&](u8 m)->u8{
        P=(m&0x80)?(P|CPU_FLAG_C):(P&~CPU_FLAG_C);
        u8 r=m<<1; setZN(r); return r;
    };
    auto LSR = [&](u8 m)->u8{
        P=(m&0x01)?(P|CPU_FLAG_C):(P&~CPU_FLAG_C);
        u8 r=m>>1; setZN(r); return r;
    };
    auto ROL = [&](u8 m)->u8{
        u8 c=P&CPU_FLAG_C;
        P=(m&0x80)?(P|CPU_FLAG_C):(P&~CPU_FLAG_C);
        u8 r=(m<<1)|c; setZN(r); return r;
    };
    auto ROR = [&](u8 m)->u8{
        u8 c=(P&CPU_FLAG_C)<<7;
        P=(m&0x01)?(P|CPU_FLAG_C):(P&~CPU_FLAG_C);
        u8 r=(m>>1)|c; setZN(r); return r;
    };
    auto BIT = [&](u8 m){
        P=(P&~(CPU_FLAG_Z|CPU_FLAG_V|CPU_FLAG_N))
         |(!(A&m)?CPU_FLAG_Z:0)
         |(m&0x40?CPU_FLAG_V:0)
         |(m&0x80?CPU_FLAG_N:0);
    };

    switch(opcode) {
    // ---- LDA ----
    case 0xA9: ADDR_IMM; A=RD; setZN(A); break;
    case 0xA5: ADDR_ZP;  A=RD; setZN(A); break;
    case 0xB5: ADDR_ZPX; A=RD; setZN(A); break;
    case 0xAD: ADDR_ABS; A=RD; setZN(A); break;
    case 0xBD: ADDR_ABSX;A=RD; setZN(A); if(extra)cyc++; break;
    case 0xB9: ADDR_ABSY;A=RD; setZN(A); if(extra)cyc++; break;
    case 0xA1: ADDR_INDX;A=RD; setZN(A); break;
    case 0xB1: ADDR_INDY;A=RD; setZN(A); if(extra)cyc++; break;
    // ---- LDX ----
    case 0xA2: ADDR_IMM; X=RD; setZN(X); break;
    case 0xA6: ADDR_ZP;  X=RD; setZN(X); break;
    case 0xB6: ADDR_ZPY; X=RD; setZN(X); break;
    case 0xAE: ADDR_ABS; X=RD; setZN(X); break;
    case 0xBE: ADDR_ABSY;X=RD; setZN(X); if(extra)cyc++; break;
    // ---- LDY ----
    case 0xA0: ADDR_IMM; Y=RD; setZN(Y); break;
    case 0xA4: ADDR_ZP;  Y=RD; setZN(Y); break;
    case 0xB4: ADDR_ZPX; Y=RD; setZN(Y); break;
    case 0xAC: ADDR_ABS; Y=RD; setZN(Y); break;
    case 0xBC: ADDR_ABSX;Y=RD; setZN(Y); if(extra)cyc++; break;
    // ---- STA ----
    case 0x85: ADDR_ZP;  write(ea,A); break;
    case 0x95: ADDR_ZPX; write(ea,A); break;
    case 0x8D: ADDR_ABS; write(ea,A); break;
    case 0x9D: ADDR_ABSX;write(ea,A); break;
    case 0x99: ADDR_ABSY;write(ea,A); break;
    case 0x81: ADDR_INDX;write(ea,A); break;
    case 0x91: ADDR_INDY;write(ea,A); break;
    // ---- STX ----
    case 0x86: ADDR_ZP;  write(ea,X); break;
    case 0x96: ADDR_ZPY; write(ea,X); break;
    case 0x8E: ADDR_ABS; write(ea,X); break;
    // ---- STY ----
    case 0x84: ADDR_ZP;  write(ea,Y); break;
    case 0x94: ADDR_ZPX; write(ea,Y); break;
    case 0x8C: ADDR_ABS; write(ea,Y); break;
    // ---- Transfer ----
    case 0xAA: X=A; setZN(X); break;
    case 0xA8: Y=A; setZN(Y); break;
    case 0x8A: A=X; setZN(A); break;
    case 0x98: A=Y; setZN(A); break;
    case 0xBA: X=SP;setZN(X); break;
    case 0x9A: SP=X;         break;
    // ---- Stack ----
    case 0x48: push(A); break;
    case 0x68: A=pop(); setZN(A); break;
    case 0x08: push(P|CPU_FLAG_B|CPU_FLAG_U); break;
    case 0x28: P=pop()|CPU_FLAG_U; break;
    // ---- ADC/SBC ----
    case 0x69: ADDR_IMM; ADC(RD); break;
    case 0x65: ADDR_ZP;  ADC(RD); break;
    case 0x75: ADDR_ZPX; ADC(RD); break;
    case 0x6D: ADDR_ABS; ADC(RD); break;
    case 0x7D: ADDR_ABSX;ADC(RD); if(extra)cyc++; break;
    case 0x79: ADDR_ABSY;ADC(RD); if(extra)cyc++; break;
    case 0x61: ADDR_INDX;ADC(RD); break;
    case 0x71: ADDR_INDY;ADC(RD); if(extra)cyc++; break;
    case 0xE9: ADDR_IMM; SBC(RD); break;
    case 0xE5: ADDR_ZP;  SBC(RD); break;
    case 0xF5: ADDR_ZPX; SBC(RD); break;
    case 0xED: ADDR_ABS; SBC(RD); break;
    case 0xFD: ADDR_ABSX;SBC(RD); if(extra)cyc++; break;
    case 0xF9: ADDR_ABSY;SBC(RD); if(extra)cyc++; break;
    case 0xE1: ADDR_INDX;SBC(RD); break;
    case 0xF1: ADDR_INDY;SBC(RD); if(extra)cyc++; break;
    // ---- AND ----
    case 0x29: ADDR_IMM; AND(RD); break;
    case 0x25: ADDR_ZP;  AND(RD); break;
    case 0x35: ADDR_ZPX; AND(RD); break;
    case 0x2D: ADDR_ABS; AND(RD); break;
    case 0x3D: ADDR_ABSX;AND(RD); if(extra)cyc++; break;
    case 0x39: ADDR_ABSY;AND(RD); if(extra)cyc++; break;
    case 0x21: ADDR_INDX;AND(RD); break;
    case 0x31: ADDR_INDY;AND(RD); if(extra)cyc++; break;
    // ---- ORA ----
    case 0x09: ADDR_IMM; ORA(RD); break;
    case 0x05: ADDR_ZP;  ORA(RD); break;
    case 0x15: ADDR_ZPX; ORA(RD); break;
    case 0x0D: ADDR_ABS; ORA(RD); break;
    case 0x1D: ADDR_ABSX;ORA(RD); if(extra)cyc++; break;
    case 0x19: ADDR_ABSY;ORA(RD); if(extra)cyc++; break;
    case 0x01: ADDR_INDX;ORA(RD); break;
    case 0x11: ADDR_INDY;ORA(RD); if(extra)cyc++; break;
    // ---- EOR ----
    case 0x49: ADDR_IMM; EOR(RD); break;
    case 0x45: ADDR_ZP;  EOR(RD); break;
    case 0x55: ADDR_ZPX; EOR(RD); break;
    case 0x4D: ADDR_ABS; EOR(RD); break;
    case 0x5D: ADDR_ABSX;EOR(RD); if(extra)cyc++; break;
    case 0x59: ADDR_ABSY;EOR(RD); if(extra)cyc++; break;
    case 0x41: ADDR_INDX;EOR(RD); break;
    case 0x51: ADDR_INDY;EOR(RD); if(extra)cyc++; break;
    // ---- BIT ----
    case 0x24: ADDR_ZP; BIT(RD); break;
    case 0x2C: ADDR_ABS;BIT(RD); break;
    // ---- CMP ----
    case 0xC9: ADDR_IMM; CMP(A,RD); break;
    case 0xC5: ADDR_ZP;  CMP(A,RD); break;
    case 0xD5: ADDR_ZPX; CMP(A,RD); break;
    case 0xCD: ADDR_ABS; CMP(A,RD); break;
    case 0xDD: ADDR_ABSX;CMP(A,RD); if(extra)cyc++; break;
    case 0xD9: ADDR_ABSY;CMP(A,RD); if(extra)cyc++; break;
    case 0xC1: ADDR_INDX;CMP(A,RD); break;
    case 0xD1: ADDR_INDY;CMP(A,RD); if(extra)cyc++; break;
    // ---- CPX/CPY ----
    case 0xE0: ADDR_IMM; CMP(X,RD); break;
    case 0xE4: ADDR_ZP;  CMP(X,RD); break;
    case 0xEC: ADDR_ABS; CMP(X,RD); break;
    case 0xC0: ADDR_IMM; CMP(Y,RD); break;
    case 0xC4: ADDR_ZP;  CMP(Y,RD); break;
    case 0xCC: ADDR_ABS; CMP(Y,RD); break;
    // ---- INC/DEC ----
    case 0xE6: ADDR_ZP; { u8 v=read(ea)+1; write(ea,v); setZN(v); } break;
    case 0xF6: ADDR_ZPX;{ u8 v=read(ea)+1; write(ea,v); setZN(v); } break;
    case 0xEE: ADDR_ABS;{ u8 v=read(ea)+1; write(ea,v); setZN(v); } break;
    case 0xFE: ADDR_ABSX;{u8 v=read(ea)+1; write(ea,v); setZN(v); } break;
    case 0xC6: ADDR_ZP; { u8 v=read(ea)-1; write(ea,v); setZN(v); } break;
    case 0xD6: ADDR_ZPX;{ u8 v=read(ea)-1; write(ea,v); setZN(v); } break;
    case 0xCE: ADDR_ABS;{ u8 v=read(ea)-1; write(ea,v); setZN(v); } break;
    case 0xDE: ADDR_ABSX;{u8 v=read(ea)-1; write(ea,v); setZN(v); } break;
    case 0xE8: X++; setZN(X); break;
    case 0xC8: Y++; setZN(Y); break;
    case 0xCA: X--; setZN(X); break;
    case 0x88: Y--; setZN(Y); break;
    // ---- ASL ----
    case 0x0A: A=ASL(A); break;
    case 0x06: ADDR_ZP; { u8 v=ASL(read(ea)); write(ea,v); } break;
    case 0x16: ADDR_ZPX;{ u8 v=ASL(read(ea)); write(ea,v); } break;
    case 0x0E: ADDR_ABS;{ u8 v=ASL(read(ea)); write(ea,v); } break;
    case 0x1E: ADDR_ABSX;{u8 v=ASL(read(ea)); write(ea,v); } break;
    // ---- LSR ----
    case 0x4A: A=LSR(A); break;
    case 0x46: ADDR_ZP; { u8 v=LSR(read(ea)); write(ea,v); } break;
    case 0x56: ADDR_ZPX;{ u8 v=LSR(read(ea)); write(ea,v); } break;
    case 0x4E: ADDR_ABS;{ u8 v=LSR(read(ea)); write(ea,v); } break;
    case 0x5E: ADDR_ABSX;{u8 v=LSR(read(ea)); write(ea,v); } break;
    // ---- ROL ----
    case 0x2A: A=ROL(A); break;
    case 0x26: ADDR_ZP; { u8 v=ROL(read(ea)); write(ea,v); } break;
    case 0x36: ADDR_ZPX;{ u8 v=ROL(read(ea)); write(ea,v); } break;
    case 0x2E: ADDR_ABS;{ u8 v=ROL(read(ea)); write(ea,v); } break;
    case 0x3E: ADDR_ABSX;{u8 v=ROL(read(ea)); write(ea,v); } break;
    // ---- ROR ----
    case 0x6A: A=ROR(A); break;
    case 0x66: ADDR_ZP; { u8 v=ROR(read(ea)); write(ea,v); } break;
    case 0x76: ADDR_ZPX;{ u8 v=ROR(read(ea)); write(ea,v); } break;
    case 0x6E: ADDR_ABS;{ u8 v=ROR(read(ea)); write(ea,v); } break;
    case 0x7E: ADDR_ABSX;{u8 v=ROR(read(ea)); write(ea,v); } break;
    // ---- JMP ----
    case 0x4C: PC=readPC16(); break;
    case 0x6C: {
        u16 ptr=readPC16();
        u8 lo=read(ptr);
        u8 hi=read((ptr&0xFF00)|((ptr+1)&0xFF)); // page wrap bug
        PC=lo|(hi<<8);
    } break;
    // ---- JSR/RTS ----
    case 0x20: { u16 target=readPC16(); push16(PC-1); PC=target; } break;
    case 0x60: PC=pop16()+1; break;
    // ---- RTI ----
    case 0x40: P=pop()|CPU_FLAG_U; PC=pop16(); break;
    // ---- Branch ----
    case 0x90: cyc+=branch(!(P&CPU_FLAG_C)); break; // BCC
    case 0xB0: cyc+=branch( (P&CPU_FLAG_C)); break; // BCS
    case 0xF0: cyc+=branch( (P&CPU_FLAG_Z)); break; // BEQ
    case 0xD0: cyc+=branch(!(P&CPU_FLAG_Z)); break; // BNE
    case 0x30: cyc+=branch( (P&CPU_FLAG_N)); break; // BMI
    case 0x10: cyc+=branch(!(P&CPU_FLAG_N)); break; // BPL
    case 0x70: cyc+=branch( (P&CPU_FLAG_V)); break; // BVS
    case 0x50: cyc+=branch(!(P&CPU_FLAG_V)); break; // BVC
    // ---- Flags ----
    case 0x18: P&=~CPU_FLAG_C; break;
    case 0x38: P|= CPU_FLAG_C; break;
    case 0x58: P&=~CPU_FLAG_I; break;
    case 0x78: P|= CPU_FLAG_I; break;
    case 0xB8: P&=~CPU_FLAG_V; break;
    case 0xD8: P&=~CPU_FLAG_D; break;
    case 0xF8: P|= CPU_FLAG_D; break;
    // ---- NOP ----
    case 0xEA: break;
    // ---- BRK ----
    case 0x00:
        readPC(); // padding byte
        push16(PC);
        push(P|CPU_FLAG_B|CPU_FLAG_U);
        P|=CPU_FLAG_I;
        { u8 lo=read(0xFFFE); u8 hi=read(0xFFFF); PC=lo|(hi<<8); }
        break;
    // ---- Undocumented NOPs (ignore) ----
    case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA: break;
    case 0x04: case 0x44: case 0x64: readPC(); break; // DOP zp
    case 0x0C: readPC16(); break; // TOP abs
    case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4: readPC(); break;
    case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:
        ADDR_ABSX; if(extra)cyc++;
        break;
    // ---- Undocumented: LAX ----
    case 0xA7: ADDR_ZP;  A=X=RD; setZN(A); break;
    case 0xB7: ADDR_ZPY; A=X=RD; setZN(A); break;
    case 0xAF: ADDR_ABS; A=X=RD; setZN(A); break;
    case 0xBF: ADDR_ABSY;A=X=RD; setZN(A); if(extra)cyc++; break;
    case 0xA3: ADDR_INDX;A=X=RD; setZN(A); break;
    case 0xB3: ADDR_INDY;A=X=RD; setZN(A); if(extra)cyc++; break;
    // ---- Undocumented: SAX ----
    case 0x87: ADDR_ZP;  write(ea,A&X); break;
    case 0x97: ADDR_ZPY; write(ea,A&X); break;
    case 0x8F: ADDR_ABS; write(ea,A&X); break;
    case 0x83: ADDR_INDX;write(ea,A&X); break;
    // ---- Undocumented: DCP ----
    case 0xC7: ADDR_ZP; { u8 v=read(ea)-1; write(ea,v); CMP(A,v); } break;
    case 0xD7: ADDR_ZPX;{ u8 v=read(ea)-1; write(ea,v); CMP(A,v); } break;
    case 0xCF: ADDR_ABS;{ u8 v=read(ea)-1; write(ea,v); CMP(A,v); } break;
    case 0xDF: ADDR_ABSX;{u8 v=read(ea)-1; write(ea,v); CMP(A,v); } break;
    case 0xDB: ADDR_ABSY;{u8 v=read(ea)-1; write(ea,v); CMP(A,v); } break;
    case 0xC3: ADDR_INDX;{u8 v=read(ea)-1; write(ea,v); CMP(A,v); } break;
    case 0xD3: ADDR_INDY;{u8 v=read(ea)-1; write(ea,v); CMP(A,v); } break;
    // ---- Undocumented: ISC ----
    case 0xE7: ADDR_ZP; { u8 v=read(ea)+1; write(ea,v); SBC(v); } break;
    case 0xF7: ADDR_ZPX;{ u8 v=read(ea)+1; write(ea,v); SBC(v); } break;
    case 0xEF: ADDR_ABS;{ u8 v=read(ea)+1; write(ea,v); SBC(v); } break;
    case 0xFF: ADDR_ABSX;{u8 v=read(ea)+1; write(ea,v); SBC(v); } break;
    case 0xFB: ADDR_ABSY;{u8 v=read(ea)+1; write(ea,v); SBC(v); } break;
    case 0xE3: ADDR_INDX;{u8 v=read(ea)+1; write(ea,v); SBC(v); } break;
    case 0xF3: ADDR_INDY;{u8 v=read(ea)+1; write(ea,v); SBC(v); } break;
    // ---- Undocumented: SLO ----
    case 0x07: ADDR_ZP; { u8 v=ASL(read(ea)); write(ea,v); ORA(v); } break;
    case 0x17: ADDR_ZPX;{ u8 v=ASL(read(ea)); write(ea,v); ORA(v); } break;
    case 0x0F: ADDR_ABS;{ u8 v=ASL(read(ea)); write(ea,v); ORA(v); } break;
    case 0x1F: ADDR_ABSX;{u8 v=ASL(read(ea)); write(ea,v); ORA(v); } break;
    case 0x1B: ADDR_ABSY;{u8 v=ASL(read(ea)); write(ea,v); ORA(v); } break;
    case 0x03: ADDR_INDX;{u8 v=ASL(read(ea)); write(ea,v); ORA(v); } break;
    case 0x13: ADDR_INDY;{u8 v=ASL(read(ea)); write(ea,v); ORA(v); } break;
    // ---- Undocumented: RLA ----
    case 0x27: ADDR_ZP; { u8 v=ROL(read(ea)); write(ea,v); AND(v); } break;
    case 0x37: ADDR_ZPX;{ u8 v=ROL(read(ea)); write(ea,v); AND(v); } break;
    case 0x2F: ADDR_ABS;{ u8 v=ROL(read(ea)); write(ea,v); AND(v); } break;
    case 0x3F: ADDR_ABSX;{u8 v=ROL(read(ea)); write(ea,v); AND(v); } break;
    case 0x3B: ADDR_ABSY;{u8 v=ROL(read(ea)); write(ea,v); AND(v); } break;
    case 0x23: ADDR_INDX;{u8 v=ROL(read(ea)); write(ea,v); AND(v); } break;
    case 0x33: ADDR_INDY;{u8 v=ROL(read(ea)); write(ea,v); AND(v); } break;
    // ---- Undocumented: SRE ----
    case 0x47: ADDR_ZP; { u8 v=LSR(read(ea)); write(ea,v); EOR(v); } break;
    case 0x57: ADDR_ZPX;{ u8 v=LSR(read(ea)); write(ea,v); EOR(v); } break;
    case 0x4F: ADDR_ABS;{ u8 v=LSR(read(ea)); write(ea,v); EOR(v); } break;
    case 0x5F: ADDR_ABSX;{u8 v=LSR(read(ea)); write(ea,v); EOR(v); } break;
    case 0x5B: ADDR_ABSY;{u8 v=LSR(read(ea)); write(ea,v); EOR(v); } break;
    case 0x43: ADDR_INDX;{u8 v=LSR(read(ea)); write(ea,v); EOR(v); } break;
    case 0x53: ADDR_INDY;{u8 v=LSR(read(ea)); write(ea,v); EOR(v); } break;
    // ---- Undocumented: RRA ----
    case 0x67: ADDR_ZP; { u8 v=ROR(read(ea)); write(ea,v); ADC(v); } break;
    case 0x77: ADDR_ZPX;{ u8 v=ROR(read(ea)); write(ea,v); ADC(v); } break;
    case 0x6F: ADDR_ABS;{ u8 v=ROR(read(ea)); write(ea,v); ADC(v); } break;
    case 0x7F: ADDR_ABSX;{u8 v=ROR(read(ea)); write(ea,v); ADC(v); } break;
    case 0x7B: ADDR_ABSY;{u8 v=ROR(read(ea)); write(ea,v); ADC(v); } break;
    case 0x63: ADDR_INDX;{u8 v=ROR(read(ea)); write(ea,v); ADC(v); } break;
    case 0x73: ADDR_INDY;{u8 v=ROR(read(ea)); write(ea,v); ADC(v); } break;

    default:
        // Unknown opcode: treat as 2-cycle NOP
        cyc = 2;
        break;
    }

    cycles += cyc;

    // Check memory (read/write) breakpoint hit
    if (memBreakpointHit_) {
        memBreakpointHit_ = false;
        return cyc;  // Pause — MainWindow will detect via checkBreakpoints
    }

    return cyc;
}

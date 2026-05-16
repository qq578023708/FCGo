#include "nes_console.h"
#include <stdexcept>
#include <cstdio>

NESConsole::NESConsole() {
    // Wire CPU bus
    bus_.ppu    = &ppu_;
    bus_.apu    = &apu_;

    // Wire PPU callbacks (PPU reads/writes CHR + nametable via ppuBus_)
    ppu_.setReadCallback([this](u16 addr) -> u8 {
        return ppuBus_.read(addr);
    });
    ppu_.setWriteCallback([this](u16 addr, u8 val) {
        ppuBus_.write(addr, val);
    });
    ppu_.setNMICallback([this]() {
        cpu_.nmi();
    });

    // Wire APU callbacks
    apu_.setMemReadCallback([this](u16 addr) -> u8 {
        return bus_.read(addr);
    });
    apu_.setIRQCallback([this]() {
        cpu_.irq();
    });

    // Connect CPU to bus
    cpu_.connect(&bus_);
}

bool NESConsole::loadROM(const std::string& path) {
    NESRom newRom;
    if (!NESRom::load(path, newRom)) return false;

    rom_       = std::move(newRom);
    mapper_    = Mapper::create(rom_);
    romLoaded_ = true;
    romPath_   = path;

    // Detect VS System mode (bit 0 of flag7 indicates VS System)
    ppu_.isVSMode = (rom_.header[7] & 0x01) != 0;

    // Wire mapper to buses
    bus_.mapper    = mapper_.get();
    ppuBus_.mapper = mapper_.get();
    ppuBus_.ppu    = &ppu_;

    // Wire mapper IRQ to CPU
    mapper_->setIRQCallback([this]() { cpu_.irq(); });

    return true;
}

void NESConsole::powerOn() {
    if (!romLoaded_) return;
    cpu_.setBreakpointManager(&breakpoints_);
    apu_.powerOn();
    ppu_.powerOn();
    memset(bus_.ram, 0xFF, sizeof(bus_.ram));
    cpu_.reset();
}

void NESConsole::reset() {
    if (!romLoaded_) return;
    apu_.reset();
    ppu_.reset();
    cpu_.reset();
}

// ============================================================
// runFrame — run CPU/PPU/APU until one frame completes
// ============================================================
void NESConsole::runFrame() {
    ppu_.frameReady = false;

    while (!ppu_.frameReady) {
        // Handle pending DMA stall
        if (bus_.dmaCycles_ > 0) {
            int stall = bus_.dmaCycles_;
            bus_.dmaCycles_ = 0;
            for (int i = 0; i < stall; i++) {
                ppu_.tick(PPU_PER_CPU);
                apu_.tick();
            }
            continue;
        }

        // Step CPU one instruction
        int cpuCycles = cpu_.step();

        // Check memory breakpoint hit (write/read)
        if (cpu_.getMemBPHit()) {
            memBPHitAddr_ = cpu_.getPC();
            return;  // Early exit — let UI handle the breakpoint
        }

        // Tick PPU 3x per CPU cycle
        ppu_.tick(cpuCycles * PPU_PER_CPU);

        // Tick APU once per CPU cycle
        for (int i = 0; i < cpuCycles; i++) apu_.tick();

        // Mapper scanline IRQ (fire when PPU scanline changes)
        static int lastScanline = -1;
        if (ppu_.scanline != lastScanline) {
            lastScanline = ppu_.scanline;
            if (mapper_) mapper_->scanlineIRQ();
        }
    }
}

// ============================================================
// State Serialization (for netplay sync)
// ============================================================

// Helper: append raw bytes to vector
static void appendBytes(std::vector<u8>& v, const void* data, size_t len) {
    const u8* p = static_cast<const u8*>(data);
    v.insert(v.end(), p, p + len);
}

// Helper: read raw bytes from vector at offset
static void readBytes(const std::vector<u8>& v, size_t& off, void* data, size_t len) {
    if (off + len > v.size()) return;
    std::memcpy(data, v.data() + off, len);
    off += len;
}

std::vector<u8> NESConsole::saveState() const {
    std::vector<u8> s;
    s.reserve(4096);

    // Magic + version
    appendBytes(s, "FCST", 4);
    u8 version = 1;
    appendBytes(s, &version, 1);

    // CPU state
    appendBytes(s, &cpu_.A, 1);
    appendBytes(s, &cpu_.X, 1);
    appendBytes(s, &cpu_.Y, 1);
    appendBytes(s, &cpu_.SP, 1);
    appendBytes(s, &cpu_.P, 1);
    appendBytes(s, &cpu_.PC, 2);
    appendBytes(s, &cpu_.cycles, 8);
    appendBytes(s, &cpu_.pendingIRQ, 4);
    appendBytes(s, &cpu_.nmiPending, 1);

    // Bus RAM (2KB)
    appendBytes(s, bus_.ram, 2048);

    // PPU state
    appendBytes(s, &ppu_.ctrl_, 1);
    appendBytes(s, &ppu_.mask_, 1);
    appendBytes(s, &ppu_.status_, 1);
    appendBytes(s, &ppu_.oamAddr_, 1);
    appendBytes(s, &ppu_.fineX_, 1);
    appendBytes(s, &ppu_.v_, 2);
    appendBytes(s, &ppu_.t_, 2);
    appendBytes(s, &ppu_.wToggle_, 1);
    appendBytes(s, &ppu_.readBuffer_, 1);
    appendBytes(s, &ppu_.oddFrame_, 1);
    appendBytes(s, &ppu_.totalCycles_, 4);
    appendBytes(s, &ppu_.scanline, 4);
    appendBytes(s, &ppu_.dot, 4);
    appendBytes(s, &ppu_.spriteCount_, 1);
    appendBytes(s, ppu_.spriteBuf_, sizeof(ppu_.spriteBuf_));
    appendBytes(s, ppu_.spritePatLo_, 8);
    appendBytes(s, ppu_.spritePatHi_, 8);
    appendBytes(s, &ppu_.bgPatLo_, 2);
    appendBytes(s, &ppu_.bgPatHi_, 2);
    appendBytes(s, &ppu_.bgAttrLo_, 2);
    appendBytes(s, &ppu_.bgAttrHi_, 2);
    appendBytes(s, &ppu_.bgAttrLatch_, 1);
    appendBytes(s, &ppu_.bgNTLatch_, 1);
    appendBytes(s, &ppu_.bgAttrByteLatch_, 1);
    appendBytes(s, &ppu_.bgPatLoLatch_, 1);
    appendBytes(s, &ppu_.bgPatHiLatch_, 1);
    // PPU RAM
    appendBytes(s, ppu_.paletteRAM, 32);
    appendBytes(s, ppu_.oam, 256);
    appendBytes(s, ppu_.nameTableRAM, 2048);

    // APU state
    appendBytes(s, &apu_.pulse_[0], sizeof(PulseChannel));
    appendBytes(s, &apu_.pulse_[1], sizeof(PulseChannel));
    appendBytes(s, &apu_.triangle_, sizeof(TriangleChannel));
    appendBytes(s, &apu_.noise_, sizeof(NoiseChannel));
    appendBytes(s, &apu_.cpuCycles_, 8);
    appendBytes(s, &apu_.frameIRQ_, 1);
    appendBytes(s, &apu_.frameCounter_, 1);
    appendBytes(s, &apu_.fiveStep_, 1);
    appendBytes(s, &apu_.inhibitIRQ_, 1);

    // Mapper SRAM (8KB)
    if (mapper_) appendBytes(s, mapper_->sram_, 8192);

    // CHR-RAM (if present, 8KB)
    if (rom_.hasCHR_RAM()) appendBytes(s, rom_.chrRAM.data(), rom_.chrRAM.size());

    return s;
}

void NESConsole::loadState(const std::vector<u8>& data) {
    if (data.size() < 5 || std::memcmp(data.data(), "FCST", 4) != 0) return;
    size_t off = 4;
    u8 version;
    readBytes(data, off, &version, 1);
    if (version != 1) return;

    // CPU
    readBytes(data, off, &cpu_.A, 1);
    readBytes(data, off, &cpu_.X, 1);
    readBytes(data, off, &cpu_.Y, 1);
    readBytes(data, off, &cpu_.SP, 1);
    readBytes(data, off, &cpu_.P, 1);
    readBytes(data, off, &cpu_.PC, 2);
    readBytes(data, off, &cpu_.cycles, 8);
    readBytes(data, off, &cpu_.pendingIRQ, 4);
    readBytes(data, off, &cpu_.nmiPending, 1);

    // Bus RAM
    readBytes(data, off, bus_.ram, 2048);

    // PPU
    readBytes(data, off, &ppu_.ctrl_, 1);
    readBytes(data, off, &ppu_.mask_, 1);
    readBytes(data, off, &ppu_.status_, 1);
    readBytes(data, off, &ppu_.oamAddr_, 1);
    readBytes(data, off, &ppu_.fineX_, 1);
    readBytes(data, off, &ppu_.v_, 2);
    readBytes(data, off, &ppu_.t_, 2);
    readBytes(data, off, &ppu_.wToggle_, 1);
    readBytes(data, off, &ppu_.readBuffer_, 1);
    readBytes(data, off, &ppu_.oddFrame_, 1);
    readBytes(data, off, &ppu_.totalCycles_, 4);
    readBytes(data, off, &ppu_.scanline, 4);
    readBytes(data, off, &ppu_.dot, 4);
    readBytes(data, off, &ppu_.spriteCount_, 1);
    readBytes(data, off, ppu_.spriteBuf_, sizeof(ppu_.spriteBuf_));
    readBytes(data, off, ppu_.spritePatLo_, 8);
    readBytes(data, off, ppu_.spritePatHi_, 8);
    readBytes(data, off, &ppu_.bgPatLo_, 2);
    readBytes(data, off, &ppu_.bgPatHi_, 2);
    readBytes(data, off, &ppu_.bgAttrLo_, 2);
    readBytes(data, off, &ppu_.bgAttrHi_, 2);
    readBytes(data, off, &ppu_.bgAttrLatch_, 1);
    readBytes(data, off, &ppu_.bgNTLatch_, 1);
    readBytes(data, off, &ppu_.bgAttrByteLatch_, 1);
    readBytes(data, off, &ppu_.bgPatLoLatch_, 1);
    readBytes(data, off, &ppu_.bgPatHiLatch_, 1);
    readBytes(data, off, ppu_.paletteRAM, 32);
    readBytes(data, off, ppu_.oam, 256);
    readBytes(data, off, ppu_.nameTableRAM, 2048);

    // APU
    readBytes(data, off, &apu_.pulse_[0], sizeof(PulseChannel));
    readBytes(data, off, &apu_.pulse_[1], sizeof(PulseChannel));
    readBytes(data, off, &apu_.triangle_, sizeof(TriangleChannel));
    readBytes(data, off, &apu_.noise_, sizeof(NoiseChannel));
    readBytes(data, off, &apu_.cpuCycles_, 8);
    readBytes(data, off, &apu_.frameIRQ_, 1);
    readBytes(data, off, &apu_.frameCounter_, 1);
    readBytes(data, off, &apu_.fiveStep_, 1);
    readBytes(data, off, &apu_.inhibitIRQ_, 1);

    // Mapper SRAM
    if (mapper_) readBytes(data, off, mapper_->sram_, 8192);

    // CHR-RAM
    if (rom_.hasCHR_RAM() && off + rom_.chrRAM.size() <= data.size()) {
        std::memcpy(rom_.chrRAM.data(), data.data() + off, rom_.chrRAM.size());
    }
}

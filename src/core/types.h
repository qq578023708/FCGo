#pragma once
#include <cstdint>
#include <cstring>
#include <functional>

// Basic NES types
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;

// NES screen dimensions
static constexpr int NES_SCREEN_WIDTH  = 256;
static constexpr int NES_SCREEN_HEIGHT = 240;
static constexpr int NES_FPS           = 60;

// CPU clock: NTSC ~1.789773 MHz
static constexpr double CPU_FREQ_NTSC  = 1789773.0;
// PPU runs at 3x CPU clock
static constexpr int    PPU_PER_CPU    = 3;
// Cycles per scanline (PPU)
static constexpr int    PPU_DOTS_PER_SCANLINE = 341;
// Total scanlines per frame (NTSC)
static constexpr int    TOTAL_SCANLINES = 262;

// IRQ/NMI flags
static constexpr u8 CPU_FLAG_C = 0x01;
static constexpr u8 CPU_FLAG_Z = 0x02;
static constexpr u8 CPU_FLAG_I = 0x04;
static constexpr u8 CPU_FLAG_D = 0x08;
static constexpr u8 CPU_FLAG_B = 0x10;
static constexpr u8 CPU_FLAG_U = 0x20;
static constexpr u8 CPU_FLAG_V = 0x40;
static constexpr u8 CPU_FLAG_N = 0x80;

// Interrupt types
enum class Interrupt { None, NMI, IRQ, Reset };

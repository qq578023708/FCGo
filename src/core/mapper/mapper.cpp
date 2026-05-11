#include "mapper.h"
#include "mappers/mapper0.h"
#include "mappers/mapper1.h"
#include "mappers/mapper2.h"
#include "mappers/mapper3.h"
#include "mappers/mapper4.h"
#include <stdexcept>
#include <fstream>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

// Helper: read entire file into vector (supports UTF-8 paths on Windows)
static bool readFile(const std::string& path, std::vector<u8>& out) {
#ifdef _WIN32
    // Convert UTF-8 to UTF-16 for Windows API
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return false;
    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);

    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size)) {
        CloseHandle(hFile);
        return false;
    }
    if (size.QuadPart > 0x10000000) { // 256MB limit
        CloseHandle(hFile);
        return false;
    }

    out.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    BOOL ok = ReadFile(hFile, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
    CloseHandle(hFile);
    return ok && read == out.size();
#else
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    out.resize(size);
    f.read((char*)out.data(), size);
    return f.good();
#endif
}

// ============================================================
// NESRom::load — iNES 1.0 / 2.0 parser
// ============================================================
bool NESRom::load(const std::string& path, NESRom& rom) {
    std::vector<u8> data;
    if (!readFile(path, data)) return false;
    if (data.size() < 16) return false;

    const u8* p = data.data();
    size_t pos = 0;

    // Validate NES magic
    if (p[0]!='N'||p[1]!='E'||p[2]!='S'||p[3]!=0x1A) return false;

    rom.prgBanks = p[4];
    rom.chrBanks = p[5];

    u8 flag6 = p[6];
    u8 flag7 = p[7];

    rom.hasBattery = (flag6>>1)&1;
    rom.hasTrainer = (flag6>>2)&1;
    bool fourScreen = (flag6>>3)&1;

    if (fourScreen) rom.mirror = MirrorType::FourScreen;
    else if (flag6&1) rom.mirror = MirrorType::Vertical;
    else rom.mirror = MirrorType::Horizontal;

    // iNES 2.0 detection
    bool ines2 = ((flag7 & 0x0C) == 0x08);
    if (ines2) {
        rom.mapperNum = (flag7 & 0xF0) | (flag6 >> 4) | ((p[8] & 0x0F) << 8);
    } else {
        rom.mapperNum = (flag7 & 0xF0) | (flag6 >> 4);
    }

    pos = 16;
    if (rom.hasTrainer) pos += 512;

    // Load PRG-ROM
    size_t prgSize = rom.prgBanks * 0x4000;
    if (pos + prgSize > data.size()) return false;
    rom.prgROM.assign(p + pos, p + pos + prgSize);
    pos += prgSize;

    // Load CHR-ROM
    if (rom.chrBanks > 0) {
        size_t chrSize = rom.chrBanks * 0x2000;
        if (pos + chrSize > data.size()) return false;
        rom.chrROM.assign(p + pos, p + pos + chrSize);
    } else {
        // CHR-RAM: 8KB
        rom.chrRAM.resize(0x2000, 0);
    }

    return true;
}

// ============================================================
// Mapper::mirrorAddress — handles all mirroring modes
// ============================================================
u16 Mapper::mirrorAddress(u16 addr) const {
    addr &= 0x0FFF;
    switch (rom_.mirror) {
    case MirrorType::Horizontal:
        return (addr & ~0x0400) | ((addr & 0x0800) ? 0x0400 : 0);
    case MirrorType::Vertical:
        return addr & 0x07FF;
    case MirrorType::FourScreen:
        return addr;
    case MirrorType::SingleLow:
        return addr & 0x03FF;
    case MirrorType::SingleHigh:
        return 0x0400 | (addr & 0x03FF);
    default:
        return addr;
    }
}

// ============================================================
// Mapper Factory
// ============================================================
std::unique_ptr<Mapper> Mapper::create(NESRom& rom) {
    switch (rom.mapperNum) {
    case 0:  return std::make_unique<Mapper0>(rom);
    case 1:  return std::make_unique<Mapper1>(rom);
    case 2:  return std::make_unique<Mapper2>(rom);
    case 3:  return std::make_unique<Mapper3>(rom);
    case 4:  return std::make_unique<Mapper4>(rom);
    default:
        // Fallback: NROM
        return std::make_unique<Mapper0>(rom);
    }
}

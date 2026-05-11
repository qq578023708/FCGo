#include "ppu.h"
#include <cstring>
#include <algorithm>

// ============================================================
// NES Standard NTSC Palette (ARGB8888)
// ============================================================
const u32 PPU::kNESPalette[64] = {
    0xFF545454,0xFF001E74,0xFF081090,0xFF300088,0xFF440064,0xFF5C0030,0xFF540400,0xFF3C1800,
    0xFF202A00,0xFF083A00,0xFF004000,0xFF003C00,0xFF00323C,0xFF000000,0xFF000000,0xFF000000,
    0xFF989698,0xFF084CC4,0xFF3032EC,0xFF5C1EE4,0xFF8814B0,0xFFA01464,0xFF982220,0xFF783C00,
    0xFF545A00,0xFF287200,0xFF087C00,0xFF007628,0xFF006678,0xFF000000,0xFF000000,0xFF000000,
    0xFFECEEEC,0xFF4C9AEC,0xFF787CEC,0xFFB062EC,0xFFE454EC,0xFFEC58B4,0xFFEC6A64,0xFFD48820,
    0xFFA0AA00,0xFF74C400,0xFF4CD020,0xFF38CC6C,0xFF38B4CC,0xFF3C3C3C,0xFF000000,0xFF000000,
    0xFFECEEEC,0xFFA8CCEC,0xFFBCBCEC,0xFFD4B2EC,0xFFECAEEC,0xFFECAED4,0xFFECB4B0,0xFFE4C490,
    0xFFCCD278,0xFFB4DE78,0xFFA8E290,0xFF98E2B4,0xFFA0D6E4,0xFFA0A2A0,0xFF000000,0xFF000000
};

// ============================================================
// PPUCTRL bits
#define CTRL_NMI        0x80
#define CTRL_MASTER     0x40
#define CTRL_SPR_SIZE   0x20  // 0=8x8, 1=8x16
#define CTRL_BG_PT      0x10  // BG pattern table 0=0x0000 1=0x1000
#define CTRL_SPR_PT     0x08  // Sprite pattern table
#define CTRL_ADDR_INC   0x04  // 0=+1(horizontal), 1=+32(vertical)
#define CTRL_NT_SEL     0x03  // Nametable select

// PPUMASK bits
#define MASK_BLUE       0x80
#define MASK_GREEN      0x40
#define MASK_RED        0x20
#define MASK_SHOW_SPR   0x10
#define MASK_SHOW_BG    0x08
#define MASK_CLIP_SPR   0x04
#define MASK_CLIP_BG    0x02
#define MASK_GRAYSCALE  0x01

// PPUSTATUS bits
#define STATUS_VBLANK   0x80
#define STATUS_SPR0_HIT 0x40
#define STATUS_SPR_OVF  0x20

// ============================================================
void PPU::powerOn() {
    memset(framebuffer, 0, sizeof(framebuffer));
    memset(paletteRAM,  0, sizeof(paletteRAM));
    memset(oam,         0, sizeof(oam));
    reset();
}

void PPU::reset() {
    ctrl_ = mask_ = status_ = oamAddr_ = 0;
    v_ = t_ = 0; wToggle_ = false;
    fineX_ = 0; readBuffer_ = 0;
    scanline = 0; dot = 0; oddFrame_ = false;
    totalCycles_ = 0; frameReady = false;
    spriteCount_ = 0;
    bgPatLo_ = bgPatHi_ = bgAttrLo_ = bgAttrHi_ = 0;
}

// ---- Bus helpers ----
u8 PPU::ppuRead(u16 addr) const {
    addr &= 0x3FFF;
    if (addr < 0x2000)      return busRead_ ? busRead_(addr) : 0;
    if (addr < 0x3F00)      return busRead_ ? busRead_(addr) : 0;
    // Palette RAM (mirrored 0x3F00-0x3FFF)
    addr &= 0x1F;
    // Mirrors: $3F10/$3F14/$3F18/$3F1C -> $3F00/$3F04/$3F08/$3F0C
    if (addr == 0x10 || addr == 0x14 || addr == 0x18 || addr == 0x1C) addr &= 0x0F;
    u8 val = paletteRAM[addr];
    return (mask_ & MASK_GRAYSCALE) ? val & 0x30 : val;
}

void PPU::ppuWrite(u16 addr, u8 val) {
    addr &= 0x3FFF;
    if (addr < 0x2000) { if(busWrite_) busWrite_(addr, val); return; }
    if (addr < 0x3F00) { if(busWrite_) busWrite_(addr, val); return; }
    addr &= 0x1F;
    if (addr == 0x10 || addr == 0x14 || addr == 0x18 || addr == 0x1C) addr &= 0x0F;
    paletteRAM[addr] = val;
}

u32 PPU::getPaletteColor(u8 idx) const {
    return kNESPalette[idx & 0x3F];
}

// ---- CPU register access ----
u8 PPU::regRead(u16 addr) {
    switch (addr & 0x7) {
    case 2: { // PPUSTATUS
        u8 s = status_;
        status_ &= ~STATUS_VBLANK;
        wToggle_ = false;
        return s;
    }
    case 4: return oam[oamAddr_]; // OAMDATA
    case 7: { // PPUDATA
        u8 buf = readBuffer_;
        readBuffer_ = ppuRead(v_);
        if ((v_ & 0x3FFF) >= 0x3F00) buf = readBuffer_; // palette: no delay
        v_ += (ctrl_ & CTRL_ADDR_INC) ? 32 : 1;
        v_ &= 0x7FFF;
        return buf;
    }
    default: return 0;
    }
}

void PPU::regWrite(u16 addr, u8 val) {
    switch (addr & 0x7) {
    case 0: // PPUCTRL
        ctrl_ = val;
        t_ = (t_ & 0xF3FF) | ((val & 0x03) << 10);
        break;
    case 1: mask_ = val; break;
    case 3: oamAddr_ = val; break;
    case 4: oam[oamAddr_++] = val; break;
    case 5: // PPUSCROLL
        if (!wToggle_) {
            t_ = (t_ & ~0x001F) | ((val >> 3) & 0x1F);
            fineX_ = val & 0x07;
        } else {
            t_ = (t_ & ~0x73E0) | ((val & 0xF8) << 2) | ((val & 0x07) << 12);
        }
        wToggle_ = !wToggle_;
        break;
    case 6: // PPUADDR
        if (!wToggle_) {
            t_ = (t_ & 0x00FF) | ((val & 0x3F) << 8);
        } else {
            t_ = (t_ & 0xFF00) | val;
            v_ = t_;
        }
        wToggle_ = !wToggle_;
        break;
    case 7:
        ppuWrite(v_, val);
        v_ += (ctrl_ & CTRL_ADDR_INC) ? 32 : 1;
        v_ &= 0x7FFF;
        break;
    }
}

void PPU::oamDMA(const u8* data) {
    memcpy(oam, data, 256);
}

// ---- Scroll helpers ----
void PPU::incrementVX() {
    if ((v_ & 0x001F) == 31) { v_ &= ~0x001F; v_ ^= 0x0400; }
    else v_++;
}
void PPU::incrementVY() {
    if ((v_ & 0x7000) != 0x7000) { v_ += 0x1000; }
    else {
        v_ &= ~0x7000;
        int y = (v_ & 0x03E0) >> 5;
        if (y == 29) { y = 0; v_ ^= 0x0800; }
        else if (y == 31) y = 0;
        else y++;
        v_ = (v_ & ~0x03E0) | (y << 5);
    }
}
void PPU::transferHorizV() { v_ = (v_ & ~0x041F) | (t_ & 0x041F); }
void PPU::transferVertV()  { v_ = (v_ & ~0x7BE0) | (t_ & 0x7BE0); }

// ---- BG helpers ----
bool PPU::renderingEnabled() const {
    return (mask_ & (MASK_SHOW_BG | MASK_SHOW_SPR)) != 0;
}

u8 PPU::fetchNT() const {
    return ppuRead(0x2000 | (v_ & 0x0FFF));
}
u8 PPU::fetchAT() const {
    u16 a = 0x23C0 | (v_ & 0x0C00) | ((v_ >> 4) & 0x38) | ((v_ >> 2) & 0x07);
    u8 shift = ((v_ >> 4) & 4) | (v_ & 2);
    return (ppuRead(a) >> shift) & 3;
}
u8 PPU::fetchBGLo() const {
    u16 fine = (v_ >> 12) & 7;
    u16 pt   = (ctrl_ & CTRL_BG_PT) ? 0x1000 : 0;
    return ppuRead(pt | (bgNTLatch_ << 4) | fine);
}
u8 PPU::fetchBGHi() const {
    u16 fine = (v_ >> 12) & 7;
    u16 pt   = (ctrl_ & CTRL_BG_PT) ? 0x1000 : 0;
    return ppuRead(pt | (bgNTLatch_ << 4) | fine | 8);
}

void PPU::loadBGShiftRegisters() {
    // Load fetched pattern data into lower 8 bits of shift registers
    bgPatLo_  = (bgPatLo_  & 0xFF00) | bgPatLoLatch_;
    bgPatHi_  = (bgPatHi_  & 0xFF00) | bgPatHiLatch_;
    // Load attribute (palette) into lower 8 bits
    bgAttrLo_ = (bgAttrLo_ & 0xFF00) | ((bgAttrByteLatch_ & 1) ? 0xFF : 0);
    bgAttrHi_ = (bgAttrHi_ & 0xFF00) | ((bgAttrByteLatch_ & 2) ? 0xFF : 0);
}

void PPU::shiftBGRegisters() {
    bgPatLo_  <<= 1;
    bgPatHi_  <<= 1;
    bgAttrLo_ <<= 1;
    bgAttrHi_ <<= 1;
}

// ---- Sprite evaluation ----
void PPU::evaluateSprites() {
    int sprH = (ctrl_ & CTRL_SPR_SIZE) ? 16 : 8;
    spriteCount_ = 0;
    memset(spriteBuf_, 0xFF, sizeof(spriteBuf_));

    for (int i = 0; i < 64 && spriteCount_ < 8; i++) {
        int oamIdx = i * 4;
        int sprY = (int)oam[oamIdx];
        if (scanline < sprY || scanline >= sprY + sprH) continue;

        spriteBuf_[spriteCount_][0] = oam[oamIdx+0]; // Y
        spriteBuf_[spriteCount_][1] = oam[oamIdx+1]; // Tile
        spriteBuf_[spriteCount_][2] = oam[oamIdx+2]; // Attr
        spriteBuf_[spriteCount_][3] = oam[oamIdx+3]; // X

        // Fetch pattern
        int tileRow = scanline - sprY;
        u8 attr = oam[oamIdx+2];
        bool flipV = (attr >> 7) & 1;
        bool flipH = (attr >> 6) & 1;
        u8 tile = oam[oamIdx+1];
        u16 pt;
        int row;
        if (sprH == 8) {
            pt = (ctrl_ & CTRL_SPR_PT) ? 0x1000 : 0;
            row = flipV ? 7 - tileRow : tileRow;
        } else {
            pt = (tile & 1) ? 0x1000 : 0;
            tile &= 0xFE;
            if (flipV) tileRow = 15 - tileRow;
            if (tileRow >= 8) { tile++; tileRow -= 8; }
            row = tileRow;
        }
        u8 lo = ppuRead(pt | (tile<<4) | row);
        u8 hi = ppuRead(pt | (tile<<4) | row | 8);

        if (flipH) {
            // Reverse bits
            auto rev = [](u8 b)->u8{
                b=(b&0xF0)>>4|(b&0x0F)<<4;
                b=(b&0xCC)>>2|(b&0x33)<<2;
                b=(b&0xAA)>>1|(b&0x55)<<1;
                return b;
            };
            lo = rev(lo); hi = rev(hi);
        }
        spritePatLo_[spriteCount_] = lo;
        spritePatHi_[spriteCount_] = hi;
        spriteCount_++;
    }
    if (spriteCount_ >= 8) status_ |= STATUS_SPR_OVF;
}

// ---- Pixel rendering ----
void PPU::renderPixel() {
    int x = dot - 1;
    int y = scanline;
    if (x < 0 || x >= NES_SCREEN_WIDTH || y < 0 || y >= NES_SCREEN_HEIGHT) return;

    // BG pixel
    u8 bgPixel  = 0;
    u8 bgPalette= 0;
    if (mask_ & MASK_SHOW_BG) {
        if (x >= 8 || !(mask_ & MASK_CLIP_BG)) {
            u16 bit = 0x8000 >> fineX_;
            bgPixel   = ((bgPatLo_ & bit) ? 1 : 0) | ((bgPatHi_ & bit) ? 2 : 0);
            bgPalette = ((bgAttrLo_ & bit) ? 1 : 0) | ((bgAttrHi_ & bit) ? 2 : 0);
        }
    }

    // Sprite pixel
    u8 sprPixel=0, sprPalette=0, sprPriority=0;
    bool isSpr0 = false;
    if (mask_ & MASK_SHOW_SPR) {
        for (int s = 0; s < (int)spriteCount_; s++) {
            int sprX = (int)spriteBuf_[s][3];
            int sx = x - sprX;
            if (sx < 0 || sx > 7) continue;
            u8 bit = 0x80 >> sx;
            u8 p = ((spritePatLo_[s] & bit) ? 1 : 0) | ((spritePatHi_[s] & bit) ? 2 : 0);
            if (p == 0) continue;
            if (x < 8 && (mask_ & MASK_CLIP_SPR)) break;
            sprPixel    = p;
            sprPalette  = (spriteBuf_[s][2] & 0x03) + 4;
            sprPriority = (spriteBuf_[s][2] >> 5) & 1;
            if (s == 0) isSpr0 = true;
            break;
        }
    }

    // Sprite0 hit
    if (isSpr0 && bgPixel && sprPixel && x < 255)
        status_ |= STATUS_SPR0_HIT;

    // Multiplexer
    u8 pixel=0, palette=0;
    if (!bgPixel && !sprPixel)      { pixel=0; palette=0; }
    else if (!bgPixel && sprPixel)  { pixel=sprPixel; palette=sprPalette; }
    else if (bgPixel && !sprPixel)  { pixel=bgPixel; palette=bgPalette; }
    else {
        if (!sprPriority) { pixel=sprPixel; palette=sprPalette; }
        else              { pixel=bgPixel;  palette=bgPalette; }
    }

    u8 colorIdx = ppuRead(0x3F00 + (pixel ? (palette<<2)|pixel : 0));
    framebuffer[y * NES_SCREEN_WIDTH + x] = getPaletteColor(colorIdx);
}

// ---- Background fetch per dot ----
void PPU::fetchBackground() {
    // Fetch happens in 8-cycle units
    int cycle = dot & 7;
    switch (cycle) {
    case 1: bgNTLatch_ = fetchNT(); break;
    case 3: bgAttrByteLatch_ = fetchAT(); break;
    case 5: bgPatLoLatch_ = fetchBGLo(); break;  // Store to latch, not shift register
    case 7: bgPatHiLatch_ = fetchBGHi(); break;  // Store to latch, not shift register
    case 0: // End of 8-cycle unit: load shift registers, inc hori(v)
        loadBGShiftRegisters();
        incrementVX();
        break;
    }
}

// ============================================================
// Main PPU tick — runs 1 PPU cycle
// ============================================================
void PPU::tick(int ppuCycles) {
    for (int i = 0; i < ppuCycles; i++) {
        bool rendering = renderingEnabled();

        // ---- Pre-render scanline (-1 / 261) ----
        if (scanline == 261) {
            if (dot == 1) {
                status_ &= ~(STATUS_VBLANK | STATUS_SPR0_HIT | STATUS_SPR_OVF);
            }
            if (rendering) {
                if (dot >= 280 && dot <= 304) transferVertV();
                if (dot >= 321 && dot <= 336) fetchBackground();
                if (dot == 256) incrementVY();
                if (dot == 257) transferHorizV();
            }
        }
        // ---- Visible scanlines (0-239) ----
        else if (scanline < 240) {
            if (dot == 0) {
                // Idle
            } else if (dot <= 256) {
                if (rendering) {
                    shiftBGRegisters();
                    fetchBackground();
                    if (dot == 256) incrementVY();
                }
                renderPixel();
            } else if (dot == 257) {
                if (rendering) transferHorizV();
                evaluateSprites(); // secondary OAM evaluation
            } else if (dot >= 321 && dot <= 336) {
                if (rendering) fetchBackground();
            }
        }
        // ---- VBlank (241) ----
        else if (scanline == 241 && dot == 1) {
            status_ |= STATUS_VBLANK;
            frameReady = true;
            if (ctrl_ & CTRL_NMI && nmiCallback_) nmiCallback_();
        }

        // ---- Advance dot/scanline ----
        dot++;
        if (dot > 340) {
            dot = 0;
            scanline++;
            if (scanline > 261) {
                scanline = 0;
                oddFrame_ = !oddFrame_;
                // Skip dot 0 on odd frames when rendering
                if (oddFrame_ && rendering) dot = 1;
            }
        }
    }
}

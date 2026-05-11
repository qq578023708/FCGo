#include "ppu_viewer.h"
#include "../../core/nes_console.h"
#include "../../core/ppu/ppu.h"
#include "../../core/memory/bus.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>

// NES colors (2C02 palette approximation)
static const uint32_t nesPalette[64] = {
    0xFF666666, 0xFF002A88, 0xFF1412A7, 0xFF3B00A4,
    0xFF5C007E, 0xFF6E0040, 0xFF6C0600, 0xFF561D00,
    0xFF333500, 0xFF0B4800, 0xFF005200, 0xFF004F08,
    0xFF00404D, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFADADAD, 0xFF155FD9, 0xFF4240FF, 0xFF7527FE,
    0xFFA01ACC, 0xFFB71E7B, 0xFFB53120, 0xFF994E00,
    0xFF6B6D00, 0xFF388700, 0xFF0C9300, 0xFF008F32,
    0xFF007C8D, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFF64B0FF, 0xFF9290FF, 0xFFC676FF,
    0xFFF36AFF, 0xFFFE6ECC, 0xFFFE8170, 0xFFEA9E22,
    0xFFBCBE00, 0xFF88D800, 0xFF5CE430, 0xFF45E082,
    0xFF48CDDE, 0xFF4F4F4F, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFFC0DFFF, 0xFFD3D2FF, 0xFFE8C8FF,
    0xFFFBC2FF, 0xFFFEC4EA, 0xFFFECCC5, 0xFFF7D8A5,
    0xFFE4E594, 0xFFCFEF96, 0xFFBDF4AB, 0xFFB3F3CC,
    0xFFB5EBF2, 0xFFB8B8B8, 0xFF000000, 0xFF000000
};

// ============================================================
// Pattern Table Widget
// ============================================================
PatternTableWidget::PatternTableWidget(NESConsole* console, int tableIndex, QWidget* parent)
    : QWidget(parent)
    , console_(console)
    , tableIndex_(tableIndex)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    imageLabel_ = new QLabel();
    imageLabel_->setMinimumSize(256, 240);
    image_ = QImage(128, 128, QImage::Format_RGB32);
    layout->addWidget(imageLabel_);
    
    refresh();
}

void PatternTableWidget::refresh() {
    if (!console_) return;
    
    const PPU& ppu = console_->getPPU();
    const auto& bus = console_->getBus();
    
    // CHR data is accessed through mapper's ppuRead
    // Pattern table 0 at $0000-$0FFF, table 1 at $1000-$1FFF
    
    for (int tileY = 0; tileY < 16; tileY++) {
        for (int tileX = 0; tileX < 16; tileX++) {
            int tileIndex = tileY * 16 + tileX;
            int tileOffset = tableIndex_ * 0x1000 + tileIndex * 16;
            
            // Each tile is 16 bytes (8x8 pixels, 2 bits per pixel)
            for (int row = 0; row < 8; row++) {
                uint16_t addr = tileOffset + row;
                uint8_t lo = 0, hi = 0;
                
                // Read CHR data via mapper
                if (bus.mapper) {
                    lo = bus.mapper->ppuRead(addr);
                    hi = bus.mapper->ppuRead(addr + 8);
                }
                
                for (int col = 0; col < 8; col++) {
                    int bit = 7 - col;
                    int color = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
                    
                    int px = tileX * 8 + col;
                    int py = tileY * 8 + row;
                    
                    uint32_t rgb = nesPalette[color * 4];  // Use palette 0
                    image_.setPixel(px, py, rgb);
                }
            }
        }
    }
    
    imageLabel_->setPixmap(QPixmap::fromImage(image_.scaled(256, 256, Qt::KeepAspectRatio)));
}

// ============================================================
// Name Table Widget
// ============================================================
NameTableWidget::NameTableWidget(NESConsole* console, QWidget* parent)
    : QWidget(parent)
    , console_(console)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    imageLabel_ = new QLabel();
    imageLabel_->setMinimumSize(512, 480);
    image_ = QImage(512, 480, QImage::Format_RGB32);
    layout->addWidget(imageLabel_);
    
    refresh();
}

void NameTableWidget::refresh() {
    if (!console_) return;
    
    const PPU& ppu = console_->getPPU();
    const auto& bus = console_->getBus();
    
    // Render 4 name tables (2x2)
    for (int nt = 0; nt < 4; nt++) {
        int baseX = (nt % 2) * 256;
        int baseY = (nt / 2) * 240;
        
        const uint8_t* nameTable = &ppu.nameTableRAM[nt * 0x400];
        
        for (int row = 0; row < 30; row++) {
            for (int col = 0; col < 32; col++) {
                uint8_t tileIndex = nameTable[row * 32 + col];
                
                // Get tile data from pattern table 0 via mapper
                int tileOffset = tileIndex * 16;
                
                for (int y = 0; y < 8; y++) {
                    uint8_t lo = 0, hi = 0;
                    if (bus.mapper) {
                        lo = bus.mapper->ppuRead(tileOffset + y);
                        hi = bus.mapper->ppuRead(tileOffset + y + 8);
                    }
                    
                    for (int x = 0; x < 8; x++) {
                        int bit = 7 - x;
                        int color = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
                        
                        int px = baseX + col * 8 + x;
                        int py = baseY + row * 8 + y;
                        
                        uint32_t rgb = nesPalette[color];
                        image_.setPixel(px, py, rgb);
                    }
                }
            }
        }
    }
    
    imageLabel_->setPixmap(QPixmap::fromImage(image_));
}

// ============================================================
// OAM Widget
// ============================================================
OAMWidget::OAMWidget(NESConsole* console, QWidget* parent)
    : QWidget(parent)
    , console_(console)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    table_ = new QTableWidget(64, 6);
    table_->setHorizontalHeaderLabels({"#", "Y", "Tile", "Attr", "X", "Pos"});
    table_->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(table_);
    
    refresh();
}

void OAMWidget::refresh() {
    if (!console_) return;
    
    const PPU& ppu = console_->getPPU();
    
    for (int i = 0; i < 64; i++) {
        int y = ppu.oam[i * 4 + 0];
        int tile = ppu.oam[i * 4 + 1];
        int attr = ppu.oam[i * 4 + 2];
        int x = ppu.oam[i * 4 + 3];
        
        table_->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
        table_->setItem(i, 1, new QTableWidgetItem(QString::number(y)));
        table_->setItem(i, 2, new QTableWidgetItem(QString("%1").arg(tile, 2, 16, QLatin1Char('0')).toUpper()));
        table_->setItem(i, 3, new QTableWidgetItem(QString("%1").arg(attr, 2, 16, QLatin1Char('0')).toUpper()));
        table_->setItem(i, 4, new QTableWidgetItem(QString::number(x)));
        table_->setItem(i, 5, new QTableWidgetItem(QString("(%1,%2)").arg(x).arg(y)));
    }
}

// ============================================================
// PPU Viewer Dialog
// ============================================================
PPUViewer::PPUViewer(NESConsole* console, QWidget* parent)
    : QDialog(parent)
    , console_(console)
{
    setWindowTitle(tr("PPU Viewer"));
    setMinimumSize(600, 500);
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    tabs_ = new QTabWidget();
    
    pattern0_ = new PatternTableWidget(console, 0, this);
    pattern1_ = new PatternTableWidget(console, 1, this);
    nameTable_ = new NameTableWidget(console, this);
    oamWidget_ = new OAMWidget(console, this);
    
    tabs_->addTab(pattern0_, tr("Pattern Table 0"));
    tabs_->addTab(pattern1_, tr("Pattern Table 1"));
    tabs_->addTab(nameTable_, tr("Name Tables"));
    tabs_->addTab(oamWidget_, tr("OAM"));
    
    layout->addWidget(tabs_);
    
    // Auto-refresh timer
    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &PPUViewer::onRefresh);
    refreshTimer_->start(200);  // 5 Hz refresh
}

void PPUViewer::onRefresh() {
    pattern0_->refresh();
    pattern1_->refresh();
    nameTable_->refresh();
    oamWidget_->refresh();
}

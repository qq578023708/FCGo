#include "memory_viewer.h"
#include "../../core/nes_console.h"
#include "../../core/memory/bus.h"
#include "../../core/ppu/ppu.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QPushButton>
#include <QMessageBox>
#include <QScrollBar>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>

#include "../../core/debug/breakpoint.h"
#include "memory_search.h"

// ---------------------------------------------------------------------------
// Activity highlight colors (FCEUX-style: dark -> bright cyan)
// ---------------------------------------------------------------------------
static QColor activityColor(int level) {
    // level 0 = no highlight, level 15 = brightest
    int t = level * 17;  // 0-255
    return QColor(0, t, t + 30);
}

// ===========================================================================
// HexEditWidget
// ===========================================================================

HexEditWidget::HexEditWidget(NESConsole* console, QWidget* parent)
    : QWidget(parent)
    , console_(console)
{
    monoFont_ = QFont("Consolas", 10);
    monoFont_.setStyleHint(QFont::Monospace);

    QFontMetrics fm(monoFont_);
    charWidth_ = fm.horizontalAdvance('0');
    rowHeight_ = fm.height() + 2;

    // Allocate memory for current region (max 64KB)
    mem_.resize(0x10000);

    // Activity fade timer
    activityTimer_ = new QTimer(this);
    activityTimer_->setInterval(80);
    connect(activityTimer_, &QTimer::timeout, this, [this]() {
        bool anyActive = false;
        for (auto& b : mem_) {
            if (b.activity > 0) {
                b.activity--;
                anyActive = true;
            }
        }
        if (!anyActive) activityTimer_->stop();
        update();
    });

    // Scroll bar (narrow style)
    vScrollBar_ = new QScrollBar(Qt::Vertical, this);
    vScrollBar_->setRange(0, 0);
    vScrollBar_->setSingleStep(1);
    vScrollBar_->setPageStep(10);
    vScrollBar_->setFixedWidth(12);
    vScrollBar_->setStyleSheet(R"(
        QScrollBar:vertical {
            background: #2D2D2D;
            width: 12px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background: #666;
            min-height: 20px;
            border-radius: 4px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background: #888;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }
    )");
    connect(vScrollBar_, &QScrollBar::valueChanged, this, [this](int val) {
        firstVisibleRow_ = val;
        update();
    });

    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

int HexEditWidget::visibleRows() const {
    return height() / rowHeight_;
}

int HexEditWidget::totalRows() const {
    switch (region_) {
    case 0: return 0x0800 / 16;   // 2KB CPU RAM
    case 1: return 0x0800 / 16;   // 2KB PPU VRAM
    case 2: return 0x0100 / 16;   // 256B OAM
    case 3: return 0x8000 / 16;   // 32KB PRG ROM
    default: return 0;
    }
}

uint8_t HexEditWidget::readByte(uint16_t addr) const {
    if (!console_) return 0;
    switch (region_) {
    case 0: return console_->getBus().read(addr & 0x07FF);
    case 1: return console_->getPPU().nameTableRAM[addr & 0x07FF];
    case 2: return console_->getPPU().oam[addr & 0xFF];
    case 3:
        if (console_->getBus().mapper) return console_->getBus().mapper->cpuRead(addr);
        return 0;
    default: return 0;
    }
}

void HexEditWidget::setRegion(int region) {
    region_ = region;
    firstVisibleRow_ = 0;
    cursorRow_ = 0;
    cursorCol_ = 0;
    mem_.assign(mem_.size(), ByteState{});
    vScrollBar_->setRange(0, qMax(0, totalRows() - visibleRows()));
    vScrollBar_->setValue(0);
    refresh();
}

void HexEditWidget::gotoAddress(uint16_t addr) {
    int row = addr / 16;
    cursorRow_ = row;
    cursorCol_ = addr % 16;
    cursorInAscii_ = false;

    // Ensure visibleRows is valid (window shown)
    int vr = qMax(1, visibleRows());
    firstVisibleRow_ = qBound(0, row - vr / 2, qMax(0, totalRows() - vr));
    vScrollBar_->setValue(firstVisibleRow_);

    // Force refresh and update
    refresh();
    update();
}

void HexEditWidget::contextMenuEvent(QContextMenuEvent* event) {
    int x = event->pos().x();
    int y = event->pos().y();
    int row = y / rowHeight_;
    int memRow = firstVisibleRow_ + row;

    if (memRow < 0 || memRow >= totalRows()) return;

    // Calculate which byte column was clicked
    int col = 0;
    int hexX = addrWidth_;
    for (int c = 0; c < bytesPerRow(); c++) {
        if (c == 8) hexX += hexGap_;
        int colWidth = charWidth_ * 2 + 4;
        if (x >= hexX && x < hexX + colWidth) { col = c; break; }
        hexX += colWidth;
    }

    uint16_t addr = memRow * bytesPerRow() + col;
    QString addrStr = QString("$%1").arg(addr, 4, 16, QLatin1Char('0')).toUpper();

    QMenu menu(this);

    // Execute breakpoint (only meaningful for ROM/CPU addresses)
    QAction* execBP = menu.addAction(tr("Execute Breakpoint at %1").arg(addrStr));
    connect(execBP, &QAction::triggered, this, [this, addr]() {
        emit breakpointToggled(addr, BP_EXEC);
    });

    // Read breakpoint (watchpoint)
    QAction* readBP = menu.addAction(tr("Read Breakpoint at %1").arg(addrStr));
    connect(readBP, &QAction::triggered, this, [this, addr]() {
        emit breakpointToggled(addr, BP_READ);
    });

    // Write breakpoint (watchpoint)
    QAction* writeBP = menu.addAction(tr("Write Breakpoint at %1").arg(addrStr));
    connect(writeBP, &QAction::triggered, this, [this, addr]() {
        emit breakpointToggled(addr, BP_WRITE);
    });

    menu.addSeparator();

    // Remove all breakpoints at this address
    QAction* clearBP = menu.addAction(tr("Clear Breakpoints at %1").arg(addrStr));
    connect(clearBP, &QAction::triggered, this, [this, addr]() {
        emit breakpointToggled(addr, 0);  // 0 = clear all
    });

    menu.exec(event->globalPos());
}

void HexEditWidget::refresh() {
    // Only refresh visible region for performance
    int startRow = firstVisibleRow_;
    int endRow = qMin(startRow + visibleRows() + 1, totalRows());
    int startAddr = startRow * 16;
    int endAddr = endRow * 16;

    for (int i = startAddr; i < endAddr && i < (int)mem_.size(); i++) {
        uint8_t newVal = readByte(i);
        if (mem_[i].value != newVal) {
            mem_[i].prevValue = mem_[i].value;
            mem_[i].value = newVal;
            mem_[i].activity = 15;  // max activity
            if (!activityTimer_->isActive()) activityTimer_->start();
        }
    }
    update();
}

void HexEditWidget::setFont(const QFont& font) {
    monoFont_ = font;
    QFontMetrics fm(monoFont_);
    charWidth_ = fm.horizontalAdvance('0');
    rowHeight_ = fm.height() + 2;
    updateLayout();
}

void HexEditWidget::updateLayout() {
    int sbWidth = vScrollBar_->width();
    vScrollBar_->setGeometry(width() - sbWidth, 0, sbWidth, height());
    vScrollBar_->setRange(0, qMax(0, totalRows() - visibleRows()));
}

void HexEditWidget::scrollContentsBy(int rows) {
    firstVisibleRow_ = qBound(0, firstVisibleRow_ + rows, qMax(0, totalRows() - visibleRows()));
    vScrollBar_->setValue(firstVisibleRow_);
}

void HexEditWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 30));
    p.setFont(monoFont_);

    int y = 0;
    int bpr = bytesPerRow();

    for (int row = 0; row < visibleRows(); row++) {
        int memRow = firstVisibleRow_ + row;
        if (memRow >= totalRows()) break;

        uint16_t rowAddr = memRow * bpr;
        y = row * rowHeight_;

        // Cursor row highlight (jump target)
        if (memRow == cursorRow_) {
            p.fillRect(0, y, width() - vScrollBar_->width(), rowHeight_, QColor(0, 100, 150));
        }

        // Address
        p.setPen(QColor(100, 180, 255));
        QString addrStr = QString("%1:").arg(rowAddr, 4, 16, QLatin1Char('0')).toUpper();
        p.drawText(4, y + rowHeight_ - 4, addrStr);

        // Hex bytes - read real-time from memory
        int x = addrWidth_;
        for (int col = 0; col < bpr; col++) {
            int idx = memRow * bpr + col;
            if (idx >= (int)mem_.size()) break;

            // Real-time read
            uint8_t val = readByte(idx);
            const ByteState& bs = mem_[idx];

            // Activity highlight background (from cached activity)
            if (bs.activity > 0) {
                p.fillRect(x - 1, y, charWidth_ * 2 + 2, rowHeight_, activityColor(bs.activity));
            }

            // Cursor highlight (bright cyan for current byte)
            if (memRow == cursorRow_ && col == cursorCol_ && !cursorInAscii_) {
                p.fillRect(x - 1, y, charWidth_ * 2 + 2, rowHeight_, QColor(0, 180, 255));
            }

            // Text color
            if (bs.activity > 8) {
                p.setPen(QColor(255, 255, 255));  // bright for active
            } else {
                p.setPen(QColor(200, 200, 200));
            }

            // Extra gap between 8-byte groups
            if (col == 8) x += hexGap_;

            QString hexStr = QString("%1").arg(val, 2, 16, QLatin1Char('0')).toUpper();
            p.drawText(x, y + rowHeight_ - 4, hexStr);
            x += charWidth_ * 2 + 4;
        }

        // ASCII column - read real-time from memory
        x += asciiGap_;
        p.setPen(QColor(140, 140, 140));
        for (int col = 0; col < bpr; col++) {
            int idx = memRow * bpr + col;
            if (idx >= (int)mem_.size()) break;

            // Real-time read
            uint8_t val = readByte(idx);

            // Cursor highlight in ASCII area (bright cyan)
            if (memRow == cursorRow_ && col == cursorCol_ && cursorInAscii_) {
                p.fillRect(x - 1, y, charWidth_ + 2, rowHeight_, QColor(0, 180, 255));
            }

            if (val >= 32 && val < 127) {
                p.setPen(QColor(160, 220, 160));
                p.drawText(x, y + rowHeight_ - 4, QString(QChar(val)));
            } else {
                p.setPen(QColor(100, 100, 100));
                p.drawText(x, y + rowHeight_ - 4, ".");
            }
            x += charWidth_;
        }
    }
}

void HexEditWidget::resizeEvent(QResizeEvent*) {
    updateLayout();
}

void HexEditWidget::wheelEvent(QWheelEvent* event) {
    int delta = event->angleDelta().y();
    int steps = delta > 0 ? -3 : 3;
    scrollContentsBy(steps);
    event->accept();
}

void HexEditWidget::keyPressEvent(QKeyEvent* event) {
    int bpr = bytesPerRow();
    switch (event->key()) {
    case Qt::Key_Up:
        if (cursorRow_ > 0) {
            cursorRow_--;
            if (cursorRow_ < firstVisibleRow_) scrollContentsBy(-1);
        }
        break;
    case Qt::Key_Down:
        if (cursorRow_ < totalRows() - 1) {
            cursorRow_++;
            if (cursorRow_ >= firstVisibleRow_ + visibleRows()) scrollContentsBy(1);
        }
        break;
    case Qt::Key_Left:
        if (cursorInAscii_) {
            if (cursorCol_ > 0) cursorCol_--;
            else { cursorInAscii_ = false; }
        } else {
            if (cursorCol_ > 0) cursorCol_--;
        }
        break;
    case Qt::Key_Right:
        if (cursorInAscii_) {
            if (cursorCol_ < bpr - 1) cursorCol_++;
        } else {
            if (cursorCol_ < bpr - 1) cursorCol_++;
            else { cursorInAscii_ = true; }
        }
        break;
    case Qt::Key_Tab:
        cursorInAscii_ = !cursorInAscii_;
        break;
    case Qt::Key_PageUp:
        cursorRow_ = qMax(0, cursorRow_ - visibleRows());
        firstVisibleRow_ = qMax(0, firstVisibleRow_ - visibleRows());
        vScrollBar_->setValue(firstVisibleRow_);
        break;
    case Qt::Key_PageDown:
        cursorRow_ = qMin(totalRows() - 1, cursorRow_ + visibleRows());
        firstVisibleRow_ = qMin(qMax(0, totalRows() - visibleRows()), firstVisibleRow_ + visibleRows());
        vScrollBar_->setValue(firstVisibleRow_);
        break;
    }
    update();
}

void HexEditWidget::mousePressEvent(QMouseEvent* event) {
    int x = event->pos().x();
    int y = event->pos().y();
    int row = y / rowHeight_;
    int memRow = firstVisibleRow_ + row;

    if (memRow < 0 || memRow >= totalRows()) return;

    cursorRow_ = memRow;

    // Determine if click is in hex or ASCII area
    int asciiStartX = addrWidth_ + (bytesPerRow() * (charWidth_ * 2 + 4)) + hexGap_ + asciiGap_;
    if (x >= asciiStartX) {
        cursorInAscii_ = true;
        cursorCol_ = qBound(0, (x - asciiStartX) / charWidth_, bytesPerRow() - 1);
    } else {
        cursorInAscii_ = false;
        // Calculate column from x position
        int hexX = addrWidth_;
        for (int col = 0; col < bytesPerRow(); col++) {
            if (col == 8) hexX += hexGap_;
            int colWidth = charWidth_ * 2 + 4;
            if (x >= hexX && x < hexX + colWidth) {
                cursorCol_ = col;
                break;
            }
            hexX += colWidth;
        }
    }
    update();
}

// ===========================================================================
// MemoryViewer
// ===========================================================================

MemoryViewer::MemoryViewer(NESConsole* console, QWidget* parent)
    : QMainWindow(parent)
    , console_(console)
{
    setupMenuBar();
    setupUI();
    setAutoRefresh(true); // Enable auto-refresh by default
}

void MemoryViewer::setupMenuBar() {
    // Menu bar removed - use Tools menu in main window instead
}

void MemoryViewer::setupUI() {
    setWindowTitle(tr("Memory Viewer"));
    setMinimumSize(750, 450);

    // Dark theme
    setStyleSheet(R"(
        QMainWindow { background-color: #1E1E1E; }
        QMenuBar { background-color: #2D2D2D; color: #D4D4D4; }
        QMenuBar::item:selected { background-color: #3E3E3E; }
        QMenu { background-color: #2D2D2D; color: #D4D4D4; }
        QMenu::item:selected { background-color: #3E3E3E; }
        QLabel { color: #D4D4D4; }
        QComboBox { background-color: #3E3E3E; color: #D4D4D4; border: 1px solid #555; padding: 2px 6px; }
        QComboBox QAbstractItemView { background-color: #2D2D2D; color: #D4D4D4; }
        QSpinBox { background-color: #3E3E3E; color: #D4D4D4; border: 1px solid #555; padding: 2px 6px; }
        QLineEdit { background-color: #3E3E3E; color: #D4D4D4; border: 1px solid #555; padding: 2px 6px; }
        QPushButton { background-color: #3E3E3E; color: #D4D4D4; border: 1px solid #555; border-radius: 3px; padding: 2px 8px; }
        QPushButton:hover { background-color: #4E4E4E; }
    )");

    QWidget* central = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();

    toolbar->addWidget(new QLabel(tr("Region:")));
    regionCombo_ = new QComboBox();
    regionCombo_->addItem(tr("CPU RAM ($0000-$1FFF)"), 0);
    regionCombo_->addItem(tr("PPU VRAM ($2000-$3FFF)"), 1);
    regionCombo_->addItem(tr("OAM ($0200-$02FF)"), 2);
    regionCombo_->addItem(tr("PRG ROM ($8000-$FFFF)"), 3);
    toolbar->addWidget(regionCombo_);

    toolbar->addWidget(new QLabel(tr("Address:")));
    addressSpin_ = new QSpinBox();
    addressSpin_->setRange(0, 0xFFFF);
    addressSpin_->setDisplayIntegerBase(16);
    addressSpin_->setPrefix("0x");
    toolbar->addWidget(addressSpin_);

    QPushButton* gotoBtn = new QPushButton(tr("Go"));
    connect(gotoBtn, &QPushButton::clicked, this, &MemoryViewer::onGoto);
    toolbar->addWidget(gotoBtn);

    toolbar->addStretch();

    mainLayout->addLayout(toolbar);

    // Hex editor widget
    hexEdit_ = new HexEditWidget(console_, this);
    mainLayout->addWidget(hexEdit_, 1);

    setCentralWidget(central);

    // Connect region change
    connect(regionCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                hexEdit_->setRegion(regionCombo_->itemData(idx).toInt());
            });
}

void MemoryViewer::refresh() {
    hexEdit_->refresh();
}

void MemoryViewer::gotoAddress(uint16_t addr) {
    addressSpin_->setValue(addr);
    hexEdit_->gotoAddress(addr);
}

void MemoryViewer::setRegion(int region) {
    regionCombo_->setCurrentIndex(region);
}

void MemoryViewer::onGoto() {
    hexEdit_->gotoAddress(addressSpin_->value());
}

void MemoryViewer::setAutoRefresh(bool enabled) {
    if (enabled) {
        if (!refreshTimer_) {
            refreshTimer_ = new QTimer(this);
            connect(refreshTimer_, &QTimer::timeout, this, &MemoryViewer::refresh);
        }
        refreshTimer_->start(100); // 100ms = 10Hz refresh rate
    } else {
        if (refreshTimer_) {
            refreshTimer_->stop();
        }
    }
}

#pragma once

#include <QMainWindow>
#include <QPainter>
#include <QScrollBar>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTimer>
#include <vector>
#include <cstdint>

class NESConsole;

// ============================================================
// HexEditWidget — FCEUX-style QPainter-based hex editor
// ============================================================
class HexEditWidget : public QWidget {
    Q_OBJECT
public:
    explicit HexEditWidget(NESConsole* console, QWidget* parent = nullptr);

    void setRegion(int region);  // 0=RAM, 1=PPU, 2=OAM, 3=ROM
    void gotoAddress(uint16_t addr);
    void refresh();
    void setFont(const QFont& font);
    uint8_t readByte(uint16_t addr) const;
    int totalRows() const;

signals:
    void breakpointToggled(uint16_t addr, bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    int bytesPerRow() const { return 16; }
    int visibleRows() const;

    NESConsole* console_;
    int region_ = 0;

    // View state
    int firstVisibleRow_ = 0;
    int cursorRow_ = 0;
    int cursorCol_ = 0;  // 0-15 hex, 16 = ascii area
    bool cursorInAscii_ = false;

    // Activity highlight (FCEUX-style 16-level fade)
    struct ByteState {
        uint8_t value = 0;
        uint8_t prevValue = 0;
        int activity = 0;  // 0-15, 15=just changed, fades to 0
    };
    std::vector<ByteState> mem_;

    QFont monoFont_;
    int charWidth_ = 8;
    int rowHeight_ = 16;

    // Layout constants
    int addrWidth_ = 60;    // "XXXX:" width
    int hexGap_ = 8;        // gap between hex groups
    int asciiGap_ = 16;     // gap between hex and ascii
    int asciiWidth_ = 140;  // ASCII column width

    QScrollBar* vScrollBar_ = nullptr;
    QTimer* activityTimer_ = nullptr;

    void updateLayout();
    void scrollContentsBy(int rows);
};

// ============================================================
// MemoryViewer — Main window with toolbar
// ============================================================
class MemoryViewer : public QMainWindow {
    Q_OBJECT
public:
    explicit MemoryViewer(NESConsole* console, QWidget* parent = nullptr);
    void refresh();
    void gotoAddress(uint16_t addr);
    void setRegion(int region);
    void setAutoRefresh(bool enabled);

public slots:
    void onGoto();

private:
    void setupUI();
    void setupMenuBar();

    NESConsole* console_ = nullptr;
    HexEditWidget* hexEdit_ = nullptr;

    QComboBox* regionCombo_ = nullptr;
    QSpinBox* addressSpin_ = nullptr;
    QTimer* refreshTimer_ = nullptr;
};

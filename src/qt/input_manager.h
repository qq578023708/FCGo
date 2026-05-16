#pragma once

#include "../core/nes_console.h"
#include <QHash>
#include <QKeyEvent>
#include <QWidget>

// Button definitions
enum class NESButton : uint8_t {
    A      = 0x80,
    B      = 0x40,
    Select = 0x20,
    Start  = 0x10,
    Up     = 0x08,
    Down   = 0x04,
    Left   = 0x02,
    Right  = 0x01
};

// VS System coin button (separate from controller)
enum class VSSystemButton : uint8_t {
    Coin = 0x01  // VS 系统投币键
};

// Key binding configuration
struct KeyBinding {
    int primaryKey = 0;      // Qt key code
    int turboKey = 0;        // Turbo key (for A/B)
    int turboRate = 10;      // Turbo rate (frames per press)
};

class InputManagerQt {
public:
    InputManagerQt();

    void update(NESConsole& console, QWidget* widget);
    void handleKeyPress(QKeyEvent* event);
    void handleKeyRelease(QKeyEvent* event);

    // VS System coin button (default: F1)
    void setCoinKey(int key) { coinKey_ = key; }
    int coinKey() const { return coinKey_; }
    bool isCoinPressed() const { return coinPressed_; }

    // Key binding configuration
    void setKeyBinding(int player, NESButton button, int key);
    void setTurboKey(int player, NESButton button, int key);
    void loadBindings();
    void saveBindings();

    // Turbo settings
    void setTurboRate(int rate) { turboRate_ = rate; }
    int turboRate() const { return turboRate_; }

private:
    uint8_t getControllerState(int player) const;
    void updateTurbo();

    // Key bindings for P1 and P2
    QHash<NESButton, KeyBinding> bindings_[2];

    // Current key states
    QHash<int, bool> keyStates_;

    // Turbo state
    int turboCounter_ = 0;
    int turboRate_ = 10;
    bool turboState_ = false;

    // VS System coin button state
    int coinKey_ = Qt::Key_F1;  // Default coin key: F1
    bool coinPressed_ = false;
};

#include "input_manager.h"
#include <QSettings>
#include <QKeySequence>

// Convert key name string to Qt key code
static int keyNameToCode(const QString& name) {
    if (name.isEmpty()) return 0;
    QKeySequence seq(name);
    if (seq.isEmpty()) return 0;
    return seq[0].key();
}

InputManagerQt::InputManagerQt() {
    // Default key bindings for Player 1
    bindings_[0][NESButton::Up]     = {Qt::Key_W, 0, 10};
    bindings_[0][NESButton::Down]   = {Qt::Key_S, 0, 10};
    bindings_[0][NESButton::Left]   = {Qt::Key_A, 0, 10};
    bindings_[0][NESButton::Right]  = {Qt::Key_D, 0, 10};
    bindings_[0][NESButton::A]      = {Qt::Key_K, Qt::Key_I, 10};  // I for turbo A
    bindings_[0][NESButton::B]      = {Qt::Key_J, Qt::Key_U, 10};  // U for turbo B
    bindings_[0][NESButton::Start]  = {Qt::Key_Return, 0, 10};
    bindings_[0][NESButton::Select] = {Qt::Key_Shift, 0, 10};

    // Default key bindings for Player 2
    bindings_[1][NESButton::Up]     = {Qt::Key_Up, 0, 10};
    bindings_[1][NESButton::Down]   = {Qt::Key_Down, 0, 10};
    bindings_[1][NESButton::Left]   = {Qt::Key_Left, 0, 10};
    bindings_[1][NESButton::Right]  = {Qt::Key_Right, 0, 10};
    bindings_[1][NESButton::A]      = {Qt::Key_Period, Qt::Key_2, 10};       // . for A, 2 for turbo A
    bindings_[1][NESButton::B]      = {Qt::Key_0, Qt::Key_1, 10};           // 0 for B, 1 for turbo B
    bindings_[1][NESButton::Start]  = {Qt::Key_Plus, 0, 10};                // + for Start
    bindings_[1][NESButton::Select] = {Qt::Key_Minus, 0, 10};               // - for Select

    loadBindings();
}

void InputManagerQt::update(NESConsole& console, QWidget* widget) {
    // Update turbo state
    updateTurbo();

    // Get controller states
    uint8_t p1 = getControllerState(0);
    uint8_t p2 = getControllerState(1);

    // Apply to console
    console.setController(0, p1);
    console.setController(1, p2);

    // Apply VS System coin input
    console.setCoinInput(coinPressed_);
}

void InputManagerQt::handleKeyPress(QKeyEvent* event) {
    keyStates_[event->key()] = true;
    
    // Handle VS System coin button
    if (event->key() == coinKey_) {
        coinPressed_ = true;
    }
}

void InputManagerQt::handleKeyRelease(QKeyEvent* event) {
    keyStates_[event->key()] = false;
    
    // Handle VS System coin button
    if (event->key() == coinKey_) {
        coinPressed_ = false;
    }
}

uint8_t InputManagerQt::getControllerState(int player) const {
    uint8_t state = 0;
    int idx = player & 1;

    for (auto it = bindings_[idx].constBegin(); it != bindings_[idx].constEnd(); ++it) {
        NESButton btn = it.key();
        const KeyBinding& binding = it.value();

        bool pressed = keyStates_.value(binding.primaryKey, false);

        // Check turbo key for A/B buttons
        if (binding.turboKey != 0 && (btn == NESButton::A || btn == NESButton::B)) {
            if (keyStates_.value(binding.turboKey, false)) {
                pressed = pressed || turboState_;
            }
        }

        if (pressed) {
            state |= static_cast<uint8_t>(btn);
        }
    }

    return state;
}

void InputManagerQt::updateTurbo() {
    turboCounter_++;
    if (turboCounter_ >= turboRate_) {
        turboCounter_ = 0;
        turboState_ = !turboState_;
    }
}

void InputManagerQt::setKeyBinding(int player, NESButton button, int key) {
    bindings_[player & 1][button].primaryKey = key;
}

void InputManagerQt::setTurboKey(int player, NESButton button, int key) {
    bindings_[player & 1][button].turboKey = key;
}

void InputManagerQt::loadBindings() {
    QSettings settings("FCGo", "FCGo");
    
    // Load P1 bindings - convert string key names to Qt key codes
    settings.beginGroup("Input/P1");
    bindings_[0][NESButton::Up].primaryKey = keyNameToCode(settings.value("Up", "W").toString());
    bindings_[0][NESButton::Down].primaryKey = keyNameToCode(settings.value("Down", "S").toString());
    bindings_[0][NESButton::Left].primaryKey = keyNameToCode(settings.value("Left", "A").toString());
    bindings_[0][NESButton::Right].primaryKey = keyNameToCode(settings.value("Right", "D").toString());
    bindings_[0][NESButton::A].primaryKey = keyNameToCode(settings.value("A", "K").toString());
    bindings_[0][NESButton::B].primaryKey = keyNameToCode(settings.value("B", "J").toString());
    bindings_[0][NESButton::Start].primaryKey = keyNameToCode(settings.value("Start", "Return").toString());
    bindings_[0][NESButton::Select].primaryKey = keyNameToCode(settings.value("Select", "Shift").toString());
    bindings_[0][NESButton::A].turboKey = keyNameToCode(settings.value("TurboA", "I").toString());
    bindings_[0][NESButton::B].turboKey = keyNameToCode(settings.value("TurboB", "U").toString());
    settings.endGroup();

    // Load P2 bindings
    settings.beginGroup("Input/P2");
    bindings_[1][NESButton::Up].primaryKey = keyNameToCode(settings.value("Up", "Up").toString());
    bindings_[1][NESButton::Down].primaryKey = keyNameToCode(settings.value("Down", "Down").toString());
    bindings_[1][NESButton::Left].primaryKey = keyNameToCode(settings.value("Left", "Left").toString());
    bindings_[1][NESButton::Right].primaryKey = keyNameToCode(settings.value("Right", "Right").toString());
    bindings_[1][NESButton::A].primaryKey = keyNameToCode(settings.value("A", "Period").toString());
    bindings_[1][NESButton::B].primaryKey = keyNameToCode(settings.value("B", "0").toString());
    bindings_[1][NESButton::Start].primaryKey = keyNameToCode(settings.value("Start", "Plus").toString());
    bindings_[1][NESButton::Select].primaryKey = keyNameToCode(settings.value("Select", "Minus").toString());
    bindings_[1][NESButton::A].turboKey = keyNameToCode(settings.value("TurboA", "2").toString());
    bindings_[1][NESButton::B].turboKey = keyNameToCode(settings.value("TurboB", "1").toString());
    settings.endGroup();

    // Load turbo rate
    turboRate_ = settings.value("Input/TurboRate", 10).toInt();
}

void InputManagerQt::saveBindings() {
    QSettings settings("FCGo", "FCGo");
    
    settings.beginGroup("Input/P1");
    settings.setValue("Up", bindings_[0][NESButton::Up].primaryKey);
    settings.setValue("Down", bindings_[0][NESButton::Down].primaryKey);
    settings.setValue("Left", bindings_[0][NESButton::Left].primaryKey);
    settings.setValue("Right", bindings_[0][NESButton::Right].primaryKey);
    settings.setValue("A", bindings_[0][NESButton::A].primaryKey);
    settings.setValue("B", bindings_[0][NESButton::B].primaryKey);
    settings.setValue("Start", bindings_[0][NESButton::Start].primaryKey);
    settings.setValue("Select", bindings_[0][NESButton::Select].primaryKey);
    settings.setValue("TurboA", bindings_[0][NESButton::A].turboKey);
    settings.setValue("TurboB", bindings_[0][NESButton::B].turboKey);
    settings.endGroup();

    settings.beginGroup("Input/P2");
    settings.setValue("Up", bindings_[1][NESButton::Up].primaryKey);
    settings.setValue("Down", bindings_[1][NESButton::Down].primaryKey);
    settings.setValue("Left", bindings_[1][NESButton::Left].primaryKey);
    settings.setValue("Right", bindings_[1][NESButton::Right].primaryKey);
    settings.setValue("A", bindings_[1][NESButton::A].primaryKey);
    settings.setValue("B", bindings_[1][NESButton::B].primaryKey);
    settings.setValue("Start", bindings_[1][NESButton::Start].primaryKey);
    settings.setValue("Select", bindings_[1][NESButton::Select].primaryKey);
    settings.setValue("TurboA", bindings_[1][NESButton::A].turboKey);
    settings.setValue("TurboB", bindings_[1][NESButton::B].turboKey);
    settings.endGroup();

    settings.setValue("Input/TurboRate", turboRate_);
}

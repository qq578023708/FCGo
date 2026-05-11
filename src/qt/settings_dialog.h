#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTabWidget>
#include <QPushButton>
#include <QLabel>
#include <QKeySequenceEdit>
#include <QGroupBox>
#include <QSlider>

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void onAccept();
    void onResetDefaults();
    void onKeyBindingChanged(int player, int button);

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    void setupAudioTab();
    void setupVideoTab();
    void setupInputTab();
    void setupEmulationTab();

    QTabWidget* tabWidget_ = nullptr;

    // Audio tab
    QSlider* volumeSlider_ = nullptr;
    QLabel* volumeLabel_ = nullptr;
    QCheckBox* enableAudioCheck_ = nullptr;

    // Video tab
    QComboBox* rendererCombo_ = nullptr;
    QSpinBox* scaleSpin_ = nullptr;
    QCheckBox* vsyncCheck_ = nullptr;
    QCheckBox* fullscreenCheck_ = nullptr;
    QCheckBox* aspectRatioCheck_ = nullptr;
    QComboBox* filterCombo_ = nullptr;

    // Input tab — 10 buttons per player: Up Down Left Right A TurboA B TurboB Start Select
    static constexpr int kButtonCount = 10;
    struct KeyBindingUI {
        QLabel* label = nullptr;
        QKeySequenceEdit* keyEdit = nullptr;
    };
    KeyBindingUI p1Bindings_[kButtonCount];
    KeyBindingUI p2Bindings_[kButtonCount];
    QSpinBox* turboRateSpin_ = nullptr;
    QPushButton* resetKeysBtn_ = nullptr;

    // Emulation tab
    QCheckBox* autoSaveCheck_ = nullptr;
    QCheckBox* rewindCheck_ = nullptr;
    QSpinBox* rewindFramesSpin_ = nullptr;
    QComboBox* regionCombo_ = nullptr;
};

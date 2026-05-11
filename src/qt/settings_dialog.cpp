#include "settings_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QSettings>
#include <QKeySequence>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI() {
    setWindowTitle(tr("Settings"));
    setMinimumSize(500, 450);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    tabWidget_ = new QTabWidget(this);

    setupAudioTab();
    setupVideoTab();
    setupInputTab();
    setupEmulationTab();

    mainLayout->addWidget(tabWidget_);

    // Buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked,
            this, &SettingsDialog::onResetDefaults);
    mainLayout->addWidget(buttons);
}

void SettingsDialog::setupAudioTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);

    // Enable audio
    enableAudioCheck_ = new QCheckBox(tr("Enable Audio"));
    layout->addWidget(enableAudioCheck_);

    // Volume
    QGroupBox* volumeGroup = new QGroupBox(tr("Volume"));
    QHBoxLayout* volumeLayout = new QHBoxLayout(volumeGroup);
    
    volumeSlider_ = new QSlider(Qt::Horizontal);
    volumeSlider_->setRange(0, 100);
    volumeSlider_->setValue(100);
    volumeLabel_ = new QLabel("100%");
    
    connect(volumeSlider_, &QSlider::valueChanged, [this](int value) {
        volumeLabel_->setText(QString("%1%").arg(value));
    });
    
    volumeLayout->addWidget(volumeSlider_);
    volumeLayout->addWidget(volumeLabel_);
    layout->addWidget(volumeGroup);

    layout->addStretch();
    tabWidget_->addTab(tab, tr("Audio"));
}

void SettingsDialog::setupVideoTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);

    // Renderer
    QGroupBox* rendererGroup = new QGroupBox(tr("Renderer"));
    QFormLayout* rendererLayout = new QFormLayout(rendererGroup);
    
    rendererCombo_ = new QComboBox();
    rendererCombo_->addItem(tr("OpenGL"), 0);
    rendererCombo_->addItem(tr("SDL2 Software"), 1);
    rendererLayout->addRow(tr("Backend:"), rendererCombo_);
    
    filterCombo_ = new QComboBox();
    filterCombo_->addItem(tr("Nearest (Pixelated)"), 0);
    filterCombo_->addItem(tr("Linear (Smooth)"), 1);
    rendererLayout->addRow(tr("Filter:"), filterCombo_);
    
    layout->addWidget(rendererGroup);

    // Display
    QGroupBox* displayGroup = new QGroupBox(tr("Display"));
    QFormLayout* displayLayout = new QFormLayout(displayGroup);
    
    scaleSpin_ = new QSpinBox();
    scaleSpin_->setRange(1, 6);
    scaleSpin_->setValue(3);
    displayLayout->addRow(tr("Scale:"), scaleSpin_);
    
    vsyncCheck_ = new QCheckBox(tr("Enable VSync"));
    vsyncCheck_->setChecked(true);
    displayLayout->addRow(vsyncCheck_);
    
    fullscreenCheck_ = new QCheckBox(tr("Start Fullscreen"));
    displayLayout->addRow(fullscreenCheck_);
    
    aspectRatioCheck_ = new QCheckBox(tr("Maintain Aspect Ratio"));
    aspectRatioCheck_->setChecked(true);
    displayLayout->addRow(aspectRatioCheck_);
    
    layout->addWidget(displayGroup);

    layout->addStretch();
    tabWidget_->addTab(tab, tr("Video"));
}

void SettingsDialog::setupInputTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);

    // 10 buttons: Up Down Left Right A TurboA B TurboB Start Select
    const char* buttonNames[kButtonCount] = {
        "Up", "Down", "Left", "Right", "A", "Turbo A", "B", "Turbo B", "Start", "Select"
    };

    // Player 1
    QGroupBox* p1Group = new QGroupBox(tr("Player 1"));
    QGridLayout* p1Grid = new QGridLayout(p1Group);

    p1Grid->addWidget(new QLabel(tr("Button")), 0, 0);
    p1Grid->addWidget(new QLabel(tr("Key")), 0, 1);
    p1Grid->addWidget(new QLabel(tr("Button")), 0, 2);
    p1Grid->addWidget(new QLabel(tr("Key")), 0, 3);

    for (int i = 0; i < kButtonCount; i++) {
        p1Bindings_[i].label = new QLabel(tr(buttonNames[i]));
        p1Bindings_[i].keyEdit = new QKeySequenceEdit();

        int row = (i / 2) + 1;
        int col = (i % 2) * 2;
        p1Grid->addWidget(p1Bindings_[i].label, row, col);
        p1Grid->addWidget(p1Bindings_[i].keyEdit, row, col + 1);
    }
    layout->addWidget(p1Group);

    // Player 2
    QGroupBox* p2Group = new QGroupBox(tr("Player 2"));
    QGridLayout* p2Grid = new QGridLayout(p2Group);

    p2Grid->addWidget(new QLabel(tr("Button")), 0, 0);
    p2Grid->addWidget(new QLabel(tr("Key")), 0, 1);
    p2Grid->addWidget(new QLabel(tr("Button")), 0, 2);
    p2Grid->addWidget(new QLabel(tr("Key")), 0, 3);

    for (int i = 0; i < kButtonCount; i++) {
        p2Bindings_[i].label = new QLabel(tr(buttonNames[i]));
        p2Bindings_[i].keyEdit = new QKeySequenceEdit();

        int row = (i / 2) + 1;
        int col = (i % 2) * 2;
        p2Grid->addWidget(p2Bindings_[i].label, row, col);
        p2Grid->addWidget(p2Bindings_[i].keyEdit, row, col + 1);
    }
    layout->addWidget(p2Group);

    // Turbo settings
    QHBoxLayout* turboLayout = new QHBoxLayout();
    turboLayout->addWidget(new QLabel(tr("Turbo Rate:")));
    turboRateSpin_ = new QSpinBox();
    turboRateSpin_->setRange(1, 30);
    turboRateSpin_->setValue(10);
    turboLayout->addWidget(turboRateSpin_);
    turboLayout->addWidget(new QLabel(tr("frames")));
    turboLayout->addStretch();
    layout->addLayout(turboLayout);

    // Reset button
    resetKeysBtn_ = new QPushButton(tr("Reset to Default Keys"));
    connect(resetKeysBtn_, &QPushButton::clicked, this, &SettingsDialog::onResetDefaults);
    layout->addWidget(resetKeysBtn_);

    tabWidget_->addTab(tab, tr("Input"));
}

void SettingsDialog::setupEmulationTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);

    // Region
    QGroupBox* regionGroup = new QGroupBox(tr("Region"));
    QFormLayout* regionLayout = new QFormLayout(regionGroup);
    
    regionCombo_ = new QComboBox();
    regionCombo_->addItem(tr("NTSC (60Hz)"), 0);
    regionCombo_->addItem(tr("PAL (50Hz)"), 1);
    regionLayout->addRow(tr("Region:"), regionCombo_);
    
    layout->addWidget(regionGroup);

    // Features
    QGroupBox* featuresGroup = new QGroupBox(tr("Features"));
    QVBoxLayout* featuresLayout = new QVBoxLayout(featuresGroup);
    
    autoSaveCheck_ = new QCheckBox(tr("Auto-save SRAM on exit"));
    featuresLayout->addWidget(autoSaveCheck_);
    
    rewindCheck_ = new QCheckBox(tr("Enable rewind"));
    featuresLayout->addWidget(rewindCheck_);
    
    QHBoxLayout* rewindLayout = new QHBoxLayout();
    rewindLayout->addWidget(new QLabel(tr("Rewind buffer:")));
    rewindFramesSpin_ = new QSpinBox();
    rewindFramesSpin_->setRange(100, 3600);
    rewindFramesSpin_->setValue(600);
    rewindLayout->addWidget(rewindFramesSpin_);
    rewindLayout->addWidget(new QLabel(tr("frames")));
    rewindLayout->addStretch();
    featuresLayout->addLayout(rewindLayout);
    
    layout->addWidget(featuresGroup);

    layout->addStretch();
    tabWidget_->addTab(tab, tr("Emulation"));
}

void SettingsDialog::loadSettings() {
    QSettings settings("FCGo", "FCGo");
    
    // Audio
    enableAudioCheck_->setChecked(settings.value("Audio/Enabled", true).toBool());
    volumeSlider_->setValue(settings.value("Audio/Volume", 100).toInt());

    // Video
    rendererCombo_->setCurrentIndex(settings.value("Video/Renderer", 0).toInt());
    filterCombo_->setCurrentIndex(settings.value("Video/Filter", 0).toInt());
    scaleSpin_->setValue(settings.value("Video/Scale", 3).toInt());
    vsyncCheck_->setChecked(settings.value("Video/VSync", true).toBool());
    fullscreenCheck_->setChecked(settings.value("Video/Fullscreen", false).toBool());
    aspectRatioCheck_->setChecked(settings.value("Video/AspectRatio", true).toBool());

    // Input - P1 (10 buttons: Up Down Left Right A TurboA B TurboB Start Select)
    settings.beginGroup("Input/P1");
    p1Bindings_[0].keyEdit->setKeySequence(QKeySequence(settings.value("Up", "W").toString()));
    p1Bindings_[1].keyEdit->setKeySequence(QKeySequence(settings.value("Down", "S").toString()));
    p1Bindings_[2].keyEdit->setKeySequence(QKeySequence(settings.value("Left", "A").toString()));
    p1Bindings_[3].keyEdit->setKeySequence(QKeySequence(settings.value("Right", "D").toString()));
    p1Bindings_[4].keyEdit->setKeySequence(QKeySequence(settings.value("A", "K").toString()));
    p1Bindings_[5].keyEdit->setKeySequence(QKeySequence(settings.value("TurboA", "I").toString()));
    p1Bindings_[6].keyEdit->setKeySequence(QKeySequence(settings.value("B", "J").toString()));
    p1Bindings_[7].keyEdit->setKeySequence(QKeySequence(settings.value("TurboB", "U").toString()));
    p1Bindings_[8].keyEdit->setKeySequence(QKeySequence(settings.value("Start", "Return").toString()));
    p1Bindings_[9].keyEdit->setKeySequence(QKeySequence(settings.value("Select", "Shift").toString()));
    settings.endGroup();

    // Input - P2
    settings.beginGroup("Input/P2");
    p2Bindings_[0].keyEdit->setKeySequence(QKeySequence(settings.value("Up", "Up").toString()));
    p2Bindings_[1].keyEdit->setKeySequence(QKeySequence(settings.value("Down", "Down").toString()));
    p2Bindings_[2].keyEdit->setKeySequence(QKeySequence(settings.value("Left", "Left").toString()));
    p2Bindings_[3].keyEdit->setKeySequence(QKeySequence(settings.value("Right", "Right").toString()));
    p2Bindings_[4].keyEdit->setKeySequence(QKeySequence(settings.value("A", "X").toString()));
    p2Bindings_[5].keyEdit->setKeySequence(QKeySequence(settings.value("TurboA", "V").toString()));
    p2Bindings_[6].keyEdit->setKeySequence(QKeySequence(settings.value("B", "Z").toString()));
    p2Bindings_[7].keyEdit->setKeySequence(QKeySequence(settings.value("TurboB", "C").toString()));
    p2Bindings_[8].keyEdit->setKeySequence(QKeySequence(settings.value("Start", "Enter").toString()));
    p2Bindings_[9].keyEdit->setKeySequence(QKeySequence(settings.value("Select", "/").toString()));
    settings.endGroup();

    turboRateSpin_->setValue(settings.value("Input/TurboRate", 10).toInt());

    // Emulation
    regionCombo_->setCurrentIndex(settings.value("Emulation/Region", 0).toInt());
    autoSaveCheck_->setChecked(settings.value("Emulation/AutoSave", true).toBool());
    rewindCheck_->setChecked(settings.value("Emulation/Rewind", false).toBool());
    rewindFramesSpin_->setValue(settings.value("Emulation/RewindFrames", 600).toInt());
}

void SettingsDialog::saveSettings() {
    QSettings settings("FCGo", "FCGo");
    
    // Audio
    settings.setValue("Audio/Enabled", enableAudioCheck_->isChecked());
    settings.setValue("Audio/Volume", volumeSlider_->value());

    // Video
    settings.setValue("Video/Renderer", rendererCombo_->currentIndex());
    settings.setValue("Video/Filter", filterCombo_->currentIndex());
    settings.setValue("Video/Scale", scaleSpin_->value());
    settings.setValue("Video/VSync", vsyncCheck_->isChecked());
    settings.setValue("Video/Fullscreen", fullscreenCheck_->isChecked());
    settings.setValue("Video/AspectRatio", aspectRatioCheck_->isChecked());

    // Input - P1 (10 buttons)
    settings.beginGroup("Input/P1");
    settings.setValue("Up", p1Bindings_[0].keyEdit->keySequence().toString());
    settings.setValue("Down", p1Bindings_[1].keyEdit->keySequence().toString());
    settings.setValue("Left", p1Bindings_[2].keyEdit->keySequence().toString());
    settings.setValue("Right", p1Bindings_[3].keyEdit->keySequence().toString());
    settings.setValue("A", p1Bindings_[4].keyEdit->keySequence().toString());
    settings.setValue("TurboA", p1Bindings_[5].keyEdit->keySequence().toString());
    settings.setValue("B", p1Bindings_[6].keyEdit->keySequence().toString());
    settings.setValue("TurboB", p1Bindings_[7].keyEdit->keySequence().toString());
    settings.setValue("Start", p1Bindings_[8].keyEdit->keySequence().toString());
    settings.setValue("Select", p1Bindings_[9].keyEdit->keySequence().toString());
    settings.endGroup();

    // Input - P2 (10 buttons)
    settings.beginGroup("Input/P2");
    settings.setValue("Up", p2Bindings_[0].keyEdit->keySequence().toString());
    settings.setValue("Down", p2Bindings_[1].keyEdit->keySequence().toString());
    settings.setValue("Left", p2Bindings_[2].keyEdit->keySequence().toString());
    settings.setValue("Right", p2Bindings_[3].keyEdit->keySequence().toString());
    settings.setValue("A", p2Bindings_[4].keyEdit->keySequence().toString());
    settings.setValue("TurboA", p2Bindings_[5].keyEdit->keySequence().toString());
    settings.setValue("B", p2Bindings_[6].keyEdit->keySequence().toString());
    settings.setValue("TurboB", p2Bindings_[7].keyEdit->keySequence().toString());
    settings.setValue("Start", p2Bindings_[8].keyEdit->keySequence().toString());
    settings.setValue("Select", p2Bindings_[9].keyEdit->keySequence().toString());
    settings.endGroup();

    settings.setValue("Input/TurboRate", turboRateSpin_->value());

    // Emulation
    settings.setValue("Emulation/Region", regionCombo_->currentIndex());
    settings.setValue("Emulation/AutoSave", autoSaveCheck_->isChecked());
    settings.setValue("Emulation/Rewind", rewindCheck_->isChecked());
    settings.setValue("Emulation/RewindFrames", rewindFramesSpin_->value());
}

void SettingsDialog::onAccept() {
    saveSettings();
    accept();
}

void SettingsDialog::onResetDefaults() {
    // Audio
    enableAudioCheck_->setChecked(true);
    volumeSlider_->setValue(100);

    // Video
    rendererCombo_->setCurrentIndex(0);
    filterCombo_->setCurrentIndex(0);
    scaleSpin_->setValue(3);
    vsyncCheck_->setChecked(true);
    fullscreenCheck_->setChecked(false);
    aspectRatioCheck_->setChecked(true);

    // Input - P1 defaults (10 buttons)
    p1Bindings_[0].keyEdit->setKeySequence(QKeySequence("W"));
    p1Bindings_[1].keyEdit->setKeySequence(QKeySequence("S"));
    p1Bindings_[2].keyEdit->setKeySequence(QKeySequence("A"));
    p1Bindings_[3].keyEdit->setKeySequence(QKeySequence("D"));
    p1Bindings_[4].keyEdit->setKeySequence(QKeySequence("K"));
    p1Bindings_[5].keyEdit->setKeySequence(QKeySequence("I"));
    p1Bindings_[6].keyEdit->setKeySequence(QKeySequence("J"));
    p1Bindings_[7].keyEdit->setKeySequence(QKeySequence("U"));
    p1Bindings_[8].keyEdit->setKeySequence(QKeySequence("Return"));
    p1Bindings_[9].keyEdit->setKeySequence(QKeySequence("Shift"));

    // Input - P2 defaults (10 buttons)
    p2Bindings_[0].keyEdit->setKeySequence(QKeySequence("Up"));
    p2Bindings_[1].keyEdit->setKeySequence(QKeySequence("Down"));
    p2Bindings_[2].keyEdit->setKeySequence(QKeySequence("Left"));
    p2Bindings_[3].keyEdit->setKeySequence(QKeySequence("Right"));
    p2Bindings_[4].keyEdit->setKeySequence(QKeySequence(Qt::Key_Period));  // A = .
    p2Bindings_[5].keyEdit->setKeySequence(QKeySequence(Qt::Key_2));      // Turbo A = 2
    p2Bindings_[6].keyEdit->setKeySequence(QKeySequence(Qt::Key_0));      // B = 0
    p2Bindings_[7].keyEdit->setKeySequence(QKeySequence(Qt::Key_1));      // Turbo B = 1
    p2Bindings_[8].keyEdit->setKeySequence(QKeySequence(Qt::Key_Plus));   // Start = +
    p2Bindings_[9].keyEdit->setKeySequence(QKeySequence(Qt::Key_Minus));  // Select = -

    turboRateSpin_->setValue(10);

    // Emulation
    regionCombo_->setCurrentIndex(0);
    autoSaveCheck_->setChecked(true);
    rewindCheck_->setChecked(false);
    rewindFramesSpin_->setValue(600);
}

void SettingsDialog::onKeyBindingChanged(int player, int button) {
    // This can be used to detect conflicts
}

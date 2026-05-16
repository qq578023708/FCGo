#include "mainwindow.h"
#include "render_widget.h"
#include "settings_dialog.h"
#include "debugger/debugger_window.h"
#include "debugger/memory_viewer.h"
#include "debugger/memory_search.h"
#include "debugger/breakpoint_window.h"
#include "network/network_lobby_window.h"
#include "debugger/disassembler.h"
#include "debugger/ppu_viewer.h"
#include "../core/debug/breakpoint.h"
#include "audio/audio_factory.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QApplication>
#include <QKeyEvent>
#include <QFileInfo>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , console_(std::make_unique<NESConsole>())
    , inputManager_(std::make_unique<InputManagerQt>())
{
    setWindowTitle("FCGo NES Emulator");
    setMinimumSize(512, 480);

    // Create render widget
    renderWidget_ = new RenderWidget(this);
    setCentralWidget(renderWidget_);

    // Ensure main window has keyboard focus
    setFocusPolicy(Qt::StrongFocus);
    
    // Install event filter on render widget to forward keys to main window
    renderWidget_->installEventFilter(this);

    // Init audio via factory
    audio_ = AudioFactory::create(AudioFactory::defaultType());
    if (audio_ && !audio_->init(44100, 1)) {
        fprintf(stderr, "[MainWindow] Audio init failed, continuing without audio.\n");
        audio_.reset();
    }

    setupMenus();
    setupStatusBar();

    // Frame timer
    frameTimer_ = new QTimer(this);
    connect(frameTimer_, &QTimer::timeout, this, &MainWindow::onFrameTick);

    // Load settings
    loadSettings();
}

MainWindow::~MainWindow() {
    frameTimer_->stop();
    if (audio_) audio_->shutdown();
    saveSettings();
}

void MainWindow::setupMenus() {
    // === File Menu ===
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    
    QAction* openAct = new QAction(tr("&Open ROM..."), this);
    openAct->setShortcut(QKeySequence::Open);
    openAct->setStatusTip(tr("Open a NES ROM file"));
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpenROM);
    fileMenu->addAction(openAct);

    recentMenu_ = new QMenu(tr("&Recent ROMs"), this);
    fileMenu->addMenu(recentMenu_);

    fileMenu->addSeparator();

    QAction* exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &MainWindow::onExit);
    fileMenu->addAction(exitAct);

    // === Emulation Menu ===
    QMenu* emuMenu = menuBar()->addMenu(tr("&Emulation"));
    
    QAction* resetAct = new QAction(tr("&Reset"), this);
    resetAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    resetAct->setStatusTip(tr("Soft reset the console"));
    connect(resetAct, &QAction::triggered, this, &MainWindow::onReset);
    emuMenu->addAction(resetAct);

    QAction* hardResetAct = new QAction(tr("&Hard Reset"), this);
    hardResetAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    hardResetAct->setStatusTip(tr("Power cycle the console"));
    connect(hardResetAct, &QAction::triggered, this, &MainWindow::onHardReset);
    emuMenu->addAction(hardResetAct);

    QAction* pauseAct = new QAction(tr("&Pause"), this);
    pauseAct->setCheckable(true);
    pauseAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(pauseAct, &QAction::toggled, this, &MainWindow::onPause);
    emuMenu->addAction(pauseAct);

    emuMenu->addSeparator();

    // Save state slots
    QMenu* saveStateMenu = emuMenu->addMenu(tr("&Save State"));
    QMenu* loadStateMenu = emuMenu->addMenu(tr("&Load State"));
    
    for (int i = 1; i <= 9; i++) {
        QAction* saveAct = new QAction(tr("Slot %1").arg(i), this);
        saveAct->setShortcut(QKeySequence(Qt::Key_F1 + i - 1));
        saveAct->setData(i);
        connect(saveAct, &QAction::triggered, [this, i]() { saveSlot_ = i; onSaveState(); });
        saveStateMenu->addAction(saveAct);

        QAction* loadAct = new QAction(tr("Slot %1").arg(i), this);
        loadAct->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F1 + i - 1));
        loadAct->setData(i);
        connect(loadAct, &QAction::triggered, [this, i]() { saveSlot_ = i; onLoadState(); });
        loadStateMenu->addAction(loadAct);
    }

    emuMenu->addSeparator();

    QAction* saveStateAsAct = new QAction(tr("Save State &As..."), this);
    connect(saveStateAsAct, &QAction::triggered, this, &MainWindow::onSaveStateAs);
    emuMenu->addAction(saveStateAsAct);

    QAction* loadStateFromAct = new QAction(tr("&Load State From..."), this);
    connect(loadStateFromAct, &QAction::triggered, this, &MainWindow::onLoadState);
    emuMenu->addAction(loadStateFromAct);

    emuMenu->addSeparator();

    QAction* loadSaveRAMAct = new QAction(tr("Load &Save RAM..."), this);
    connect(loadSaveRAMAct, &QAction::triggered, this, &MainWindow::onLoadSaveRAM);
    emuMenu->addAction(loadSaveRAMAct);

    // === Tools Menu ===
    QMenu* toolsMenu = menuBar()->addMenu(tr("&Tools"));
    
    QAction* debuggerAct = new QAction(tr("&Debugger"), this);
    debuggerAct->setShortcut(QKeySequence(Qt::Key_F5));
    debuggerAct->setCheckable(true);
    connect(debuggerAct, &QAction::triggered, this, &MainWindow::onDebugger);
    toolsMenu->addAction(debuggerAct);

    QAction* bpAct = new QAction(tr("&Breakpoints"), this);
    connect(bpAct, &QAction::triggered, this, &MainWindow::onBreakpoints);
    toolsMenu->addAction(bpAct);

    QAction* netLobbyAct = new QAction(tr("&Network Lobby..."), this);
    connect(netLobbyAct, &QAction::triggered, this, &MainWindow::onNetworkLobby);
    toolsMenu->addAction(netLobbyAct);

    toolsMenu->addSeparator();

    QAction* memViewAct = new QAction(tr("&Memory Viewer"), this);
    memViewAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(memViewAct, &QAction::triggered, this, &MainWindow::onMemoryViewer);
    toolsMenu->addAction(memViewAct);

    QAction* memSearchAct = new QAction(tr("Memory &Search..."), this);
    memSearchAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    connect(memSearchAct, &QAction::triggered, this, &MainWindow::onMemorySearch);
    toolsMenu->addAction(memSearchAct);

    // PPU Viewers submenu
    QMenu* ppuMenu = toolsMenu->addMenu(tr("&PPU Viewers"));
    
    QAction* patternAct = new QAction(tr("&Pattern Tables"), this);
    connect(patternAct, &QAction::triggered, this, &MainWindow::onPatternTableViewer);
    ppuMenu->addAction(patternAct);

    QAction* nameAct = new QAction(tr("&Name Tables"), this);
    connect(nameAct, &QAction::triggered, this, &MainWindow::onNameTableViewer);
    ppuMenu->addAction(nameAct);

    QAction* oamAct = new QAction(tr("&OAM Viewer"), this);
    connect(oamAct, &QAction::triggered, this, &MainWindow::onOAMViewer);
    ppuMenu->addAction(oamAct);

    // === Settings Menu ===
    QMenu* settingsMenu = menuBar()->addMenu(tr("&Settings"));
    
    QAction* settingsAct = new QAction(tr("&Settings..."), this);
    settingsAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));  // Ctrl+, (standard settings shortcut)
    connect(settingsAct, &QAction::triggered, this, &MainWindow::onSettings);
    settingsMenu->addAction(settingsAct);

    // === Language Menu ===
    setupLanguageMenu();

    // === Help Menu ===
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    
    QAction* aboutAct = new QAction(tr("&About"), this);
    connect(aboutAct, &QAction::triggered, this, &MainWindow::onAbout);
    helpMenu->addAction(aboutAct);

    QAction* aboutQtAct = new QAction(tr("About &Qt"), this);
    connect(aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt);
    helpMenu->addAction(aboutQtAct);
}

void MainWindow::setupLanguageMenu() {
    languageMenu_ = menuBar()->addMenu(tr("&Language"));
    
    QActionGroup* langGroup = new QActionGroup(this);
    
    QAction* englishAct = new QAction("English", this);
    englishAct->setCheckable(true);
    englishAct->setChecked(true);
    englishAct->setData("en");
    langGroup->addAction(englishAct);
    languageMenu_->addAction(englishAct);
    
    QAction* chineseAct = new QAction("中文", this);
    chineseAct->setCheckable(true);
    chineseAct->setData("zh_CN");
    langGroup->addAction(chineseAct);
    languageMenu_->addAction(chineseAct);
    
    connect(langGroup, &QActionGroup::triggered, this, &MainWindow::onLanguageChanged);
}

void MainWindow::setupToolbar() {
    QToolBar* toolbar = addToolBar(tr("Main Toolbar"));
    toolbar->setObjectName("mainToolBar");
    toolbar->setMovable(false);
    
    QAction* openAct = toolbar->addAction(tr("Open"));
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpenROM);

    toolbar->addSeparator();

    QAction* resetAct = toolbar->addAction(tr("Reset"));
    connect(resetAct, &QAction::triggered, this, &MainWindow::onReset);

    QAction* pauseAct = toolbar->addAction(tr("Pause"));
    pauseAct->setCheckable(true);
    connect(pauseAct, &QAction::toggled, this, &MainWindow::onPause);

    toolbar->addSeparator();

    QAction* debuggerAct = toolbar->addAction(tr("Debugger"));
    connect(debuggerAct, &QAction::triggered, this, &MainWindow::onDebugger);
}

void MainWindow::setupStatusBar() {
    fpsLabel_ = new QLabel("FPS: 0");
    fpsLabel_->setMinimumWidth(80);
    statusBar()->addPermanentWidget(fpsLabel_);

    latencyLabel_ = new QLabel("Ping: --");
    latencyLabel_->setMinimumWidth(80);
    statusBar()->addPermanentWidget(latencyLabel_);

    stateLabel_ = new QLabel("No ROM");
    stateLabel_->setMinimumWidth(200);
    statusBar()->addWidget(stateLabel_);
}

void MainWindow::onOpenROM() {
    // Prevent loading ROM during network play (client side)
    if (netPlayActive_ && !netPlayIsHost_) {
        QMessageBox::information(this, tr("Network Play"),
            tr("Cannot load ROM during network play. Please leave the lobby first."));
        return;
    }

    QString path = QFileDialog::getOpenFileName(this,
        tr("Open NES ROM"), QString(),
        tr("NES ROM Files (*.nes *.nsf *.fds);;All Files (*.*)"));

    if (!path.isEmpty()) {
        loadROM(path);
        // Add to recent
        recentROMs_.removeAll(path);
        recentROMs_.prepend(path);
        if (recentROMs_.size() > 10) recentROMs_.removeLast();
        updateRecentMenu();
    }
}

void MainWindow::onRecentROM() {
    if (netPlayActive_ && !netPlayIsHost_) {
        QMessageBox::information(this, tr("Network Play"),
            tr("Cannot load ROM during network play. Please leave the lobby first."));
        return;
    }

    QAction* act = qobject_cast<QAction*>(sender());
    if (act) {
        QString path = act->data().toString();
        if (QFile::exists(path)) {
            loadROM(path);
        } else {
            QMessageBox::warning(this, tr("File Not Found"),
                tr("The ROM file '%1' no longer exists.").arg(path));
            recentROMs_.removeAll(path);
            updateRecentMenu();
        }
    }
}

void MainWindow::onExit() {
    close();
}

bool MainWindow::loadROM(const QString& path) {
    if (!console_->loadROM(path.toStdString())) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load ROM: %1").arg(path));
        return false;
    }

    romPath_ = path;
    romLoaded_ = true;
    console_->powerOn();
    
    frameTimer_->start(16); // ~60 FPS
    updateTitle();
    stateLabel_->setText(QFileInfo(path).fileName());

    // If in network lobby, sync ROM to clients (host side only)
    // Client side: netPlayActive_ is true and netPlayIsHost_ is false, so skip
    if (networkLobbyWindow_ && networkLobbyWindow_->isVisible() &&
        (!netPlayActive_ || netPlayIsHost_)) {
        networkLobbyWindow_->hostLoadROM(path);
        netPlayActive_ = true;
        netPlayIsHost_ = true;
    }
    
    return true;
}

void MainWindow::onReset() {
    if (romLoaded_) {
        console_->reset();
        statusBar()->showMessage(tr("Console reset"), 2000);
    }
}

void MainWindow::onHardReset() {
    if (romLoaded_) {
        console_->powerOn();
        statusBar()->showMessage(tr("Console power cycled"), 2000);
    }
}

void MainWindow::onPause(bool checked) {
    paused_ = checked;
    updateTitle();
    stateLabel_->setText(checked ? tr("[Paused] ") + QFileInfo(romPath_).fileName() 
                                  : QFileInfo(romPath_).fileName());
}

void MainWindow::onLoadState() {
    if (!romLoaded_) return;
    
    QString filename = QString("%1_slot%2.sav").arg(romPath_).arg(saveSlot_);
    // TODO: Implement state loading
    statusBar()->showMessage(tr("Loaded state from slot %1").arg(saveSlot_), 2000);
}

void MainWindow::onSaveState() {
    if (!romLoaded_) return;
    
    QString filename = QString("%1_slot%2.sav").arg(romPath_).arg(saveSlot_);
    // TODO: Implement state saving
    statusBar()->showMessage(tr("Saved state to slot %1").arg(saveSlot_), 2000);
}

void MainWindow::onSaveStateAs() {
    if (!romLoaded_) return;
    
    QString path = QFileDialog::getSaveFileName(this,
        tr("Save State"), QString(), tr("Save States (*.sav)"));
    if (!path.isEmpty()) {
        // TODO: Implement state saving
        statusBar()->showMessage(tr("Saved state to %1").arg(path), 2000);
    }
}

void MainWindow::onLoadSaveRAM() {
    if (!romLoaded_) return;
    
    QString path = QFileDialog::getOpenFileName(this,
        tr("Load Save RAM"), QString(), tr("Save RAM (*.srm)"));
    if (!path.isEmpty()) {
        // TODO: Implement Save RAM loading
        statusBar()->showMessage(tr("Loaded Save RAM"), 2000);
    }
}

void MainWindow::onDebugger() {
    if (!debuggerWindow_) {
        debuggerWindow_ = std::make_unique<DebuggerWindow>(console_.get(), this);
        // Connect debugger pause callback to main window
        debuggerWindow_->setPauseCallback([this](bool paused) {
            paused_ = paused;
            updateTitle();
            stateLabel_->setText(paused ? tr("[Paused] ") + QFileInfo(romPath_).fileName() 
                                          : QFileInfo(romPath_).fileName());
        });
    }
    debuggerWindow_->show();
    debuggerWindow_->raise();
    debuggerWindow_->activateWindow();
}


void MainWindow::onMemoryViewer() {
    if (!memoryViewer_) {
        memoryViewer_ = std::make_unique<MemoryViewer>(console_.get(), this);
        // Connect breakpoint signal from memory viewer
        QObject::connect(memoryViewer_->findChild<HexEditWidget*>(), &HexEditWidget::breakpointToggled,
            this, [this](uint16_t addr, uint16_t typeFlags) {
                BreakpointManager& bpm = console_->getBreakpoints();
                if (typeFlags == 0) {
                    // Clear all breakpoints at this address
                    for (int i = bpm.count() - 1; i >= 0; i--) {
                        if (bpm.at(i).address == addr) bpm.remove(i);
                    }
                } else {
                    bpm.toggle(addr, typeFlags);
                }
                // Sync to debugger
                if (debuggerWindow_) {
                    debuggerWindow_->syncBreakpoints(bpm);
                }
            });
    }
    memoryViewer_->show();
    memoryViewer_->raise();
    memoryViewer_->activateWindow();
}

void MainWindow::onMemorySearch() {
    if (!memorySearchDialog_) {
        memorySearchDialog_ = std::make_unique<MemorySearchDialog>(console_.get(), memoryViewer_.get(), this);
    }
    memorySearchDialog_->show();
    memorySearchDialog_->raise();
    memorySearchDialog_->activateWindow();
}

void MainWindow::onBreakpoints() {
    if (!breakpointWindow_) {
        breakpointWindow_ = std::make_unique<BreakpointWindow>(&console_->getBreakpoints(), this);
    }
    breakpointWindow_->refreshList();
    breakpointWindow_->show();
    breakpointWindow_->raise();
    breakpointWindow_->activateWindow();
}

void MainWindow::onNetworkLobby() {
    if (!networkLobbyWindow_) {
        networkLobbyWindow_ = std::make_unique<NetworkLobbyWindow>(console_.get(), this);
        connect(networkLobbyWindow_.get(), &NetworkLobbyWindow::romSyncComplete, this, [this](const QString& romPath) {
            netPlayActive_ = true;
            netPlayIsHost_ = false;
            // Reset network state for new ROM
            netFrameCount_ = 0;
            netInputHead_ = 0;
            netInputTail_ = 0;
            netNextExpectedFrame_ = 0;
            netStateBuffer_.clear();
            netStateChunksTotal_ = 0;
            netStateChunksReceived_ = 0;
            netNoDataFrames_ = 0;
            loadROM(romPath);
        });
        connect(networkLobbyWindow_.get(), &NetworkLobbyWindow::netPlayEnded, this, [this](const QString& reason) {
            netPlayActive_ = false;
            netPlayIsHost_ = false;
            netFrameCount_ = 0;
            netInputHead_ = 0;
            netInputTail_ = 0;
            netNextExpectedFrame_ = 0;
            netStateBuffer_.clear();
            netStateChunksTotal_ = 0;
            netStateChunksReceived_ = 0;
            netNewPlayerJoined_ = false;
            statusBar()->showMessage(tr("Network play ended: %1").arg(reason), 5000);
        });
        connect(networkLobbyWindow_.get(), &NetworkLobbyWindow::playerJoined, this, [this](uint64_t, const QString&) {
            if (netPlayActive_ && netPlayIsHost_) {
                netNewPlayerJoined_ = true;
            }
        });
    }
    networkLobbyWindow_->show();
}

void MainWindow::onPPUViewer() {
    if (!ppuViewer_) {
        ppuViewer_ = std::make_unique<PPUViewer>(console_.get(), this);
    }
    ppuViewer_->show();
    ppuViewer_->raise();
    ppuViewer_->activateWindow();
}

void MainWindow::onPatternTableViewer() {
    onPPUViewer();
}

void MainWindow::onNameTableViewer() {
    onPPUViewer();
}

void MainWindow::onOAMViewer() {
    onPPUViewer();
}

void MainWindow::onSettings() {
    if (!settingsDialog_) {
        settingsDialog_ = std::make_unique<SettingsDialog>(this);
    }
    if (settingsDialog_->exec() == QDialog::Accepted) {
        // Apply audio settings
        QSettings settings("FCGo", "FCGo");
        bool audioEnabled = settings.value("Audio/Enabled", true).toBool();
        int volume = settings.value("Audio/Volume", 100).toInt();

        if (audioEnabled && !audio_) {
            audio_ = AudioFactory::create(AudioFactory::defaultType());
            if (audio_) audio_->init(44100, 1);
        } else if (!audioEnabled && audio_) {
            audio_->shutdown();
            audio_.reset();
        }
        if (audio_) {
            audio_->setVolume(volume / 100.0f);
        }
        
        // Apply video settings
        int scale = settings.value("Video/Scale", 3).toInt();
        if (renderWidget_ && scale >= 1 && scale <= 6) {
            renderWidget_->setScale(scale);
        }
        
        statusBar()->showMessage(tr("Settings applied"), 2000);
    }
}

void MainWindow::onLanguageChanged(QAction* action) {
    QString lang = action->data().toString();
    QSettings settings("FCGo", "FCGo");
    settings.setValue("Language", lang);
    
    // Remove old translator and install new one
    if (translator_) {
        qApp->removeTranslator(translator_);
        delete translator_;
        translator_ = nullptr;
    }
    
    if (lang == "zh_CN") {
        translator_ = new QTranslator(this);
        if (translator_->load(":/fcgo_zh_CN.qm") ||
            translator_->load(":/translations/fcgo_zh_CN.qm") ||
            translator_->load(":/src/qt/translations/fcgo_zh_CN.qm")) {
            qApp->installTranslator(translator_);
        }
    }
    
    // Retranslate UI
    retranslateUi();
}

void MainWindow::onAbout() {
    QMessageBox::about(this, tr("About FCGo"),
        tr("<h3>FCGo NES Emulator</h3>"
           "<p>A portable NES/Famicom emulator.</p>"
           "<p>Version 1.0</p>"
           "<p>Built with Qt 6 and C++17</p>"
           "<p><a href='https://github.com'>GitHub</a></p>"));
}

void MainWindow::onFrameTick() {
    if (!paused_ && romLoaded_) {
        if (netPlayActive_ && !netPlayIsHost_) {
            // CLIENT: read local P1 keyboard input and send to host
            inputManager_->update(*console_, this);
            uint8_t localInput = console_->getControllerState(0);
            networkLobbyWindow_->steamManager()->sendClientInput(0, localInput);

            // CLIENT: receive input and state snapshots from network
            if (networkLobbyWindow_) {
                uint8_t buf[8192];
                size_t len = sizeof(buf);
                uint64_t sender = 0;
                while (networkLobbyWindow_->steamManager()->receiveNetData(buf, &len, &sender)) {
                    netNoDataFrames_ = 0;  // Reset timeout counter
                    if (len == 7 && buf[0] == 'I') {
                        // Frame input from host
                        uint32_t frameNum = buf[1] | (buf[2] << 8) | (buf[3] << 16) | (buf[4] << 24);
                        uint8_t p1 = buf[5];  // P1 from host (always use this)
                        // p2 from host is ignored - client uses local input for P2
                        if (frameNum >= netNextExpectedFrame_) {
                            int slot = netInputTail_ % NET_BUFFER_SIZE;
                            netInputBuffer_[slot] = {frameNum, p1, 0};
                            netInputTail_++;
                            netNextExpectedFrame_ = frameNum + 1;
                        }
                    } else if (len == 9 && buf[0] == 'P') {
                        // Ping from host - reply with pong
                        networkLobbyWindow_->steamManager()->handlePing(buf, len, sender);
                    } else if (len == 9 && buf[0] == 'O') {
                        // Pong from host - calculate latency
                        networkLobbyWindow_->steamManager()->handlePong(buf, len);
                    } else if (len > 13 && buf[0] == 'S') {
                        // State snapshot chunk
                        uint32_t chunkIdx = buf[1] | (buf[2] << 8) | (buf[3] << 16) | (buf[4] << 24);
                        uint32_t totalChunks = buf[5] | (buf[6] << 8) | (buf[7] << 16) | (buf[8] << 24);
                        uint32_t chunkSize = buf[9] | (buf[10] << 8) | (buf[11] << 16) | (buf[12] << 24);

                        if (chunkIdx == 0) {
                            netStateBuffer_.clear();
                            netStateChunksTotal_ = totalChunks;
                            netStateChunksReceived_ = 0;
                        }
                        if (chunkIdx == netStateChunksReceived_) {
                            netStateBuffer_.insert(netStateBuffer_.end(), buf + 13, buf + 13 + chunkSize);
                            netStateChunksReceived_++;
                        }
                        if (netStateChunksReceived_ == netStateChunksTotal_ && netStateChunksTotal_ > 0) {
                            // Full state received - load it
                            console_->loadState(netStateBuffer_);
                            // Reset input buffer to sync with host
                            netInputHead_ = 0;
                            netInputTail_ = 0;
                            netStateChunksTotal_ = 0;
                            netStateChunksReceived_ = 0;
                        }
                    } else if (len == 1 && buf[0] == 'K') {
                        // Kicked by host
                        netPlayActive_ = false;
                        netPlayIsHost_ = false;
                        netNoDataFrames_ = 0;
                        netInputHead_ = 0;
                        netInputTail_ = 0;
                        netNextExpectedFrame_ = 0;
                        netStateBuffer_.clear();
                        netStateChunksTotal_ = 0;
                        netStateChunksReceived_ = 0;
                        statusBar()->showMessage(tr("You have been kicked from the lobby"), 5000);
                    } else if (len == 1 && buf[0] == 'F') {
                        // Lobby full
                        statusBar()->showMessage(tr("The lobby is full"), 5000);
                    } else {
                        // Forward non-frame-sync packets to lobby (ROM sync, chat, etc.)
                        QByteArray data(reinterpret_cast<const char*>(buf), static_cast<int>(len));
                        networkLobbyWindow_->handleNetMessage(data);
                    }
                }
                // Update latency display
                int latency = networkLobbyWindow_->steamManager()->getLatency();
                if (latency > 0) {
                    latencyLabel_->setText(tr("Ping: %1 ms").arg(latency));
                }
            }

            // Execute buffered frames (catch up if behind)
            while (netInputHead_ < netInputTail_) {
                int slot = netInputHead_ % NET_BUFFER_SIZE;
                const auto& input = netInputBuffer_[slot];
                // P1 from host (network), P2 from local input
                console_->setController(0, input.p1);
                console_->setController(1, localInput);
                console_->runFrame();

                if (audio_) {
                    std::vector<float> samples;
                    console_->getAudioSamples(samples);
                    audio_->pushSamples(samples);
                }
                netInputHead_++;
            }

            // Render latest frame
            if (netInputHead_ > 0) {
                renderWidget_->present(console_->getFramebuffer());
            }

            // Client: send ping every 10 frames (~166ms)
            netClientFrameCount_++;
            if (netClientFrameCount_ % 10 == 0) {
                networkLobbyWindow_->steamManager()->sendPing();
            }

            // Client timeout detection: no data for ~2 seconds (120 frames)
            netNoDataFrames_++;
            if (netNoDataFrames_ > 120) {
                netPlayActive_ = false;
                netPlayIsHost_ = false;
                netNoDataFrames_ = 0;
                netInputHead_ = 0;
                netInputTail_ = 0;
                netNextExpectedFrame_ = 0;
                netStateBuffer_.clear();
                netStateChunksTotal_ = 0;
                netStateChunksReceived_ = 0;
                statusBar()->showMessage(tr("Network play ended: Connection timed out"), 5000);
            }
        } else {
            // HOST (or local): normal input + run frame

            // HOST: First, receive client input before running frame
            // Client sends P1 keyboard input (playerId=0), host maps it to P2 (controller 1)
            if (netPlayActive_ && netPlayIsHost_ && networkLobbyWindow_) {
                uint8_t buf[64];
                size_t len = sizeof(buf);
                uint64_t sender = 0;
                while (networkLobbyWindow_->steamManager()->receiveNetData(buf, &len, &sender)) {
                    if (len == 3 && buf[0] == 'C') {
                        uint8_t playerId = buf[1];
                        uint8_t buttons = buf[2];
                        if (playerId == 0) {
                            netClientP2Input_ = buttons;
                        }
                    } else if (len == 9 && buf[0] == 'O') {
                        // Pong from client - calculate latency
                        networkLobbyWindow_->steamManager()->handlePong(buf, len);
                    } else if (len == 9 && buf[0] == 'P') {
                        // Ping from client - reply with pong
                        networkLobbyWindow_->steamManager()->handlePing(buf, len, sender);
                    }
                }
            }

            // HOST: update latency display every frame
            if (networkLobbyWindow_) {
                int latency = networkLobbyWindow_->steamManager()->getLatency();
                if (latency > 0) {
                    latencyLabel_->setText(tr("Ping: %1 ms").arg(latency));
                }
            }

            inputManager_->update(*console_, this);

            // Apply client P2 input if received (overrides local P2)
            if (netPlayActive_ && netPlayIsHost_ && netClientP2Input_ != 0xFF) {
                console_->setController(1, netClientP2Input_);
                netClientP2Input_ = 0xFF;  // Reset for next frame
            }

            checkBreakpoints();
            console_->runFrame();
            checkBreakpoints();

            // Push audio samples
            if (audio_) {
                std::vector<float> samples;
                console_->getAudioSamples(samples);
                audio_->pushSamples(samples);
            }
            // Render
            renderWidget_->present(console_->getFramebuffer());

            // HOST: send input to clients with frame number
            if (netPlayActive_ && netPlayIsHost_ && networkLobbyWindow_) {
                uint8_t p1 = console_->getControllerState(0);
                uint8_t p2 = console_->getControllerState(1);
                networkLobbyWindow_->steamManager()->sendFrameInput(netFrameCount_, p1, p2);

                // Send ping every 10 frames (~166ms) to measure latency
                if (netFrameCount_ % 10 == 0) {
                    networkLobbyWindow_->steamManager()->sendPing();
                }

                // Send full state snapshot every 60 frames (1 second) to keep clients in sync
                if (netFrameCount_ % 60 == 0 || netNewPlayerJoined_) {
                    auto state = console_->saveState();
                    networkLobbyWindow_->steamManager()->sendStateSnapshot(state);
                    netNewPlayerJoined_ = false;
                }

                netFrameCount_++;
            }
        }

        // Update FPS
        fpsCounter_++;
        static int fpsTimer = 0;
        fpsTimer++;
        if (fpsTimer >= 60) {
            currentFPS_ = fpsCounter_;
            fpsCounter_ = 0;
            fpsTimer = 0;
            fpsLabel_->setText(tr("FPS: %1").arg(currentFPS_));
        }
    }
}

void MainWindow::updateTitle() {
    QString title = "FCGo NES Emulator";
    if (romLoaded_) {
        QFileInfo fi(romPath_);
        title += " - " + fi.fileName();
    }
    if (paused_) {
        title += " [Paused]";
    }
    setWindowTitle(title);
}

void MainWindow::updateRecentMenu() {
    recentMenu_->clear();
    
    if (recentROMs_.isEmpty()) {
        QAction* emptyAct = new QAction(tr("(No recent ROMs)"), this);
        emptyAct->setEnabled(false);
        recentMenu_->addAction(emptyAct);
    } else {
        for (const QString& path : recentROMs_) {
            QAction* act = new QAction(QFileInfo(path).fileName(), this);
            act->setData(path);
            act->setStatusTip(path);
            connect(act, &QAction::triggered, this, &MainWindow::onRecentROM);
            recentMenu_->addAction(act);
        }
        
        recentMenu_->addSeparator();
        QAction* clearAct = new QAction(tr("Clear Recent"), this);
        connect(clearAct, &QAction::triggered, [this]() {
            recentROMs_.clear();
            updateRecentMenu();
        });
        recentMenu_->addAction(clearAct);
    }
}

void MainWindow::saveSettings() {
    QSettings settings("FCGo", "FCGo");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("recentROMs", recentROMs_);
}

void MainWindow::loadSettings() {
    QSettings settings("FCGo", "FCGo");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    recentROMs_ = settings.value("recentROMs").toStringList();
    updateRecentMenu();
    
    // Load language
    QString lang = settings.value("Language", "en").toString();
    // 更新菜单选中状态
    if (languageMenu_) {
        for (QAction* act : languageMenu_->actions()) {
            if (act->data().toString() == lang) {
                act->setChecked(true);
                break;
            }
        }
    }
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi() {
    setWindowTitle(tr("FCGo - NES Emulator"));
    updateTitle();
    // Toolbar and status bar labels will be updated via tr() on next repaint
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (debuggerWindow_) {
        debuggerWindow_->close();
    }
    if (memoryViewer_) {
        memoryViewer_->close();
    }
    if (networkLobbyWindow_) {
        networkLobbyWindow_->close();
        networkLobbyWindow_.reset();
    }
    event->accept();
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (inputManager_) {
        inputManager_->handleKeyPress(event);
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event) {
    if (inputManager_) {
        inputManager_->handleKeyRelease(event);
    }
    QMainWindow::keyReleaseEvent(event);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    // Forward key events from render widget to main window
    if (obj == renderWidget_) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            keyPressEvent(keyEvent);
            return true;
        } else if (event->type() == QEvent::KeyRelease) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            keyReleaseEvent(keyEvent);
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::checkBreakpoints() {
    if (!console_) return;

    // Check memory breakpoint hit (write/read during runFrame)
    if (console_->hasMemBreakpointHit()) {
        uint16_t addr = console_->getMemBPHitAddr();
        console_->clearMemBPHit();
        onBreakpointHit(addr);
        return;
    }

    // Check execution breakpoint
    if (console_->getBreakpoints().isEmpty()) return;
    uint16_t pc = console_->getCPU().PC;
    int hitIdx = -1;
    if (console_->getBreakpoints().checkExec(pc, hitIdx)) {
        onBreakpointHit(pc);
    }
}

void MainWindow::onBreakpointHit(uint16_t addr) {
    // Pause emulation
    paused_ = true;
    updateTitle();
    stateLabel_->setText(tr("[Breakpoint] ") + QFileInfo(romPath_).fileName()
        + tr(" at $%1").arg(addr, 4, 16, QLatin1Char('0')).toUpper());

    // Open debugger if not already open
    if (!debuggerWindow_) {
        onDebugger();
    }

    // Sync breakpoints to debugger
    if (debuggerWindow_) {
        debuggerWindow_->syncBreakpoints(console_->getBreakpoints());
    }

    // Pause and jump to the breakpoint address
    debuggerWindow_->setPauseAndGoto(addr);

    debuggerWindow_->show();
    debuggerWindow_->raise();
    debuggerWindow_->activateWindow();
}

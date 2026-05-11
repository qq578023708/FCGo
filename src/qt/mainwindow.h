#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QSet>
#include <memory>

#include "../core/nes_console.h"
#include "input_manager.h"
#include "audio/iaudio_backend.h"

class RenderWidget;
class SettingsDialog;
class DebuggerWindow;
class MemoryViewer;
class MemorySearchDialog;
class BreakpointWindow;
class PPUViewer;
class NetworkLobbyWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    bool loadROM(const QString& path);
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    // File menu
    void onOpenROM();
    void onRecentROM();
    void onExit();

    // Emulation menu
    void onReset();
    void onPause(bool checked);
    void onHardReset();
    void onLoadState();
    void onSaveState();
    void onSaveStateAs();
    void onLoadSaveRAM();

    // Tools menu
    void onDebugger();
    void onMemoryViewer();
    void onMemorySearch();
    void onBreakpoints();
    void onNetworkLobby();
    void onPPUViewer();
    void onPatternTableViewer();
    void onNameTableViewer();
    void onOAMViewer();

    // Settings menu
    void onSettings();

    // Language menu
    void onLanguageChanged(QAction* action);

    // Help menu
    void onAbout();

    // Frame tick
    void onFrameTick();

private:
    void setupMenus();
    void setupToolbar();
    void setupStatusBar();
    void setupLanguageMenu();
    void updateTitle();
    void saveSettings();
    void loadSettings();
    void updateRecentMenu();

    // Core
    std::unique_ptr<NESConsole> console_;
    std::unique_ptr<InputManagerQt> inputManager_;
    std::unique_ptr<IAudioBackend> audio_;

    // UI
    RenderWidget* renderWidget_ = nullptr;
    QLabel* fpsLabel_ = nullptr;
    QLabel* stateLabel_ = nullptr;
    QLabel* latencyLabel_ = nullptr;  // Network latency display
    QTimer* frameTimer_ = nullptr;

    // Dialogs
    std::unique_ptr<SettingsDialog> settingsDialog_;
    std::unique_ptr<DebuggerWindow> debuggerWindow_;
    std::unique_ptr<MemoryViewer> memoryViewer_;
    std::unique_ptr<MemorySearchDialog> memorySearchDialog_;
    std::unique_ptr<BreakpointWindow> breakpointWindow_;
    std::unique_ptr<NetworkLobbyWindow> networkLobbyWindow_;
    std::unique_ptr<PPUViewer> ppuViewer_;

    // Recent ROMs
    QStringList recentROMs_;
    QMenu* recentMenu_ = nullptr;

    // Language menu
    QMenu* languageMenu_ = nullptr;

    // State
    QString romPath_;
    bool paused_ = false;
    bool romLoaded_ = false;
    int fpsCounter_ = 0;
    int currentFPS_ = 0;
    int saveSlot_ = 0;

    // Network frame sync
    bool netPlayActive_ = false;   // true when in network game
    bool netPlayIsHost_ = false;   // true if this is the host
    uint32_t netFrameCount_ = 0;   // current frame number (host)
    uint32_t netClientFrameCount_ = 0;  // client frame counter for ping timing

    // Client-side input buffer
    static constexpr int NET_BUFFER_SIZE = 60; // buffer up to 60 frames (1 second)
    struct NetInput {
        uint32_t frameNum = 0;
        uint8_t p1 = 0;
        uint8_t p2 = 0;
    };
    NetInput netInputBuffer_[NET_BUFFER_SIZE];
    int netInputHead_ = 0;         // next frame to execute
    int netInputTail_ = 0;         // next slot to write
    uint32_t netNextExpectedFrame_ = 0; // next frame number we expect

    // State snapshot receive (client)
    std::vector<u8> netStateBuffer_;
    uint32_t netStateChunksTotal_ = 0;
    uint32_t netStateChunksReceived_ = 0;

    // New player joined flag (host sends immediate snapshot)
    bool netNewPlayerJoined_ = false;

    // Client timeout detection
    int netNoDataFrames_ = 0;  // frames without receiving any data

    // Host: client P2 input for current frame (0xFF = no input)
    uint8_t netClientP2Input_ = 0xFF;

    // Breakpoints (shared between debugger and memory viewer)
    QSet<uint16_t> breakpoints_;
    void checkBreakpoints();
    void onBreakpointHit(uint16_t addr);
};

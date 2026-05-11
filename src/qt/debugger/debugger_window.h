#pragma once

#include <QMainWindow>
#include <QTableView>
#include <QTextEdit>
#include <QLabel>
#include <QSplitter>
#include <QTimer>
#include <QSet>
#include <memory>
#include <functional>

#include "../../core/debug/breakpoint.h"

class NESConsole;
class DisassemblerView;

class DebuggerWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit DebuggerWindow(NESConsole* console, QWidget* parent = nullptr);
    ~DebuggerWindow();

    // Set callback to control emulation pause state
    void setPauseCallback(std::function<void(bool)> callback) { pauseCallback_ = callback; }
    void setStepCallback(std::function<void()> callback) { stepCallback_ = callback; }
    
    // Called when emulation pauses (from external source)
    void onEmulationPaused();
    
    // Sync breakpoints from external source (memory viewer)
    void syncBreakpoints(const BreakpointManager& bpm);
    
    // Pause and jump to address (breakpoint hit)
    void setPauseAndGoto(uint16_t addr);
    
    // Update all views
    void refresh();

public slots:
    void onPauseToggled(bool checked);
    void onStep();
    void onStepOver();
    void onStepFrame();
    void onReset();
    void onAutoRefresh();

private:
    void setupUI();
    void setupInstructionHelp();
    void updateCPUStatus();
    void updateDisassembly();
    void updateInstructionHelp(uint16_t addr);

    NESConsole* console_ = nullptr;
    
    // UI components
    DisassemblerView* disasmView_ = nullptr;
    
    // 底部指令说明标签
    QLabel* instructionHelpLabel_ = nullptr;
    
    // CPU status labels
    QLabel* pcLabel_ = nullptr;
    QLabel* aLabel_ = nullptr;
    QLabel* xLabel_ = nullptr;
    QLabel* yLabel_ = nullptr;
    QLabel* spLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* scanlineLabel_ = nullptr;
    QLabel* cycleLabel_ = nullptr;
    
    // Auto-refresh timer
    QTimer* refreshTimer_ = nullptr;

    // State
    bool paused_ = false;
    uint16_t lastPC_ = 0;
    
    // Callbacks
    std::function<void(bool)> pauseCallback_;
    std::function<void()> stepCallback_;
};

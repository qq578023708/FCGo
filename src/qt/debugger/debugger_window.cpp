#include "debugger_window.h"
#include "disassembler.h"

#include "../../core/nes_console.h"
#include "../../core/cpu/cpu6502.h"
#include "../../core/ppu/ppu.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QStatusBar>
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QIcon>

DebuggerWindow::DebuggerWindow(NESConsole* console, QWidget* parent)
    : QMainWindow(parent)
    , console_(console)
{
    setupUI();
    
    // Initial refresh to show disassembly
    if (console_) {
        refresh();
    }
    
    // Auto-refresh timer (updates views while running)
    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &DebuggerWindow::onAutoRefresh);
    refreshTimer_->start(100);  // 10 Hz refresh
}

DebuggerWindow::~DebuggerWindow() = default;

void DebuggerWindow::setupUI() {
    setWindowTitle(tr("Debugger"));
    setMinimumSize(900, 600);
    
    // Apply dark theme
    setStyleSheet(R"(
        QMainWindow { background-color: #1E1E1E; }
        QMenuBar { background-color: #2D2D2D; color: #D4D4D4; }
        QMenuBar::item:selected { background-color: #3E3E3E; }
        QMenu { background-color: #2D2D2D; color: #D4D4D4; }
        QMenu::item:selected { background-color: #3E3E3E; }
        QToolBar { background-color: #2D2D2D; border: none; spacing: 2px; padding: 2px; }
        QToolButton { background-color: #3E3E3E; color: #D4D4D4; border: 1px solid #555; border-radius: 3px; padding: 2px 6px; font-size: 12px; }
        QToolButton:hover { background-color: #4E4E4E; }
        QToolButton:checked { background-color: #0078D4; }
        QGroupBox { background-color: #2D2D2D; color: #D4D4D4; border: 1px solid #3E3E3E; border-radius: 4px; margin-top: 8px; padding-top: 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QLabel { color: #D4D4D4; }
        QFormLayout { spacing: 4px; }
        QStatusBar { background-color: #2D2D2D; color: #D4D4D4; }
        QSplitter::handle { background-color: #3E3E3E; }
    )");

    // Menu bar
    QMenu* debugMenu = menuBar()->addMenu(tr("&Debug"));
    
    QAction* pauseAct = new QAction(tr("&Pause"), this);
    pauseAct->setCheckable(true);
    pauseAct->setShortcut(QKeySequence(Qt::Key_F6));
    connect(pauseAct, &QAction::toggled, this, &DebuggerWindow::onPauseToggled);
    debugMenu->addAction(pauseAct);

    QAction* stepAct = new QAction(tr("&Step"), this);
    stepAct->setShortcut(QKeySequence(Qt::Key_F7));
    connect(stepAct, &QAction::triggered, this, &DebuggerWindow::onStep);
    debugMenu->addAction(stepAct);

    QAction* stepOverAct = new QAction(tr("Step &Over"), this);
    stepOverAct->setShortcut(QKeySequence(Qt::Key_F8));
    connect(stepOverAct, &QAction::triggered, this, &DebuggerWindow::onStepOver);
    debugMenu->addAction(stepOverAct);

    QAction* stepFrameAct = new QAction(tr("Step &Frame"), this);
    stepFrameAct->setShortcut(QKeySequence(Qt::Key_F10));
    connect(stepFrameAct, &QAction::triggered, this, &DebuggerWindow::onStepFrame);
    debugMenu->addAction(stepFrameAct);

    debugMenu->addSeparator();

    QAction* resetAct = new QAction(tr("&Reset"), this);
    connect(resetAct, &QAction::triggered, this, &DebuggerWindow::onReset);
    debugMenu->addAction(resetAct);

    // Toolbar with icons
    QToolBar* toolbar = addToolBar(tr("Debug Toolbar"));
    toolbar->setObjectName("debugToolBar");
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setIconSize(QSize(8, 8));
    
    // Set icons for toolbar actions (8x8 pure color PNG)
    pauseAct->setIcon(QIcon(":/resources/icons/pause.png"));
    stepAct->setIcon(QIcon(":/resources/icons/step.png"));
    stepOverAct->setIcon(QIcon(":/resources/icons/step_over.png"));
    stepFrameAct->setIcon(QIcon(":/resources/icons/step_frame.png"));
    
    toolbar->addAction(pauseAct);
    toolbar->addAction(stepAct);
    toolbar->addAction(stepOverAct);
    toolbar->addAction(stepFrameAct);

    // Main splitter
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    // Left panel: CPU status
    QWidget* leftPanel = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);

    // CPU registers
    QGroupBox* cpuGroup = new QGroupBox(tr("CPU Registers"));
    QFormLayout* cpuLayout = new QFormLayout(cpuGroup);
    
    pcLabel_ = new QLabel("0000");
    pcLabel_->setStyleSheet("font-family: Consolas; font-size: 14px;");
    aLabel_ = new QLabel("00");
    xLabel_ = new QLabel("00");
    yLabel_ = new QLabel("00");
    spLabel_ = new QLabel("FD");
    statusLabel_ = new QLabel("NV-BDIZC");
    statusLabel_->setStyleSheet("font-family: Consolas;");
    
    cpuLayout->addRow("PC:", pcLabel_);
    cpuLayout->addRow("A:", aLabel_);
    cpuLayout->addRow("X:", xLabel_);
    cpuLayout->addRow("Y:", yLabel_);
    cpuLayout->addRow("SP:", spLabel_);
    cpuLayout->addRow("P:", statusLabel_);
    
    leftLayout->addWidget(cpuGroup);

    // PPU status
    QGroupBox* ppuGroup = new QGroupBox(tr("PPU Status"));
    QFormLayout* ppuLayout = new QFormLayout(ppuGroup);
    
    scanlineLabel_ = new QLabel("0");
    cycleLabel_ = new QLabel("0");
    
    ppuLayout->addRow("Scanline:", scanlineLabel_);
    ppuLayout->addRow("Cycle:", cycleLabel_);
    
    leftLayout->addWidget(ppuGroup);
    leftLayout->addStretch();

    splitter->addWidget(leftPanel);

    // Center panel: Disassembly with instruction help at bottom
    QWidget* centerPanel = new QWidget();
    QVBoxLayout* centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(0, 0, 0, 0);

    disasmView_ = new DisassemblerView(console_, this);
    centerLayout->addWidget(disasmView_, 1);

    // Instruction help label at bottom
    instructionHelpLabel_ = new QLabel(tr("Select an instruction to see details"));
    instructionHelpLabel_->setStyleSheet(
        "QLabel { background-color: #2D2D2D; color: #D4D4D4; "
        "padding: 8px; border-top: 1px solid #3E3E3E; }"
    );
    instructionHelpLabel_->setWordWrap(true);
    instructionHelpLabel_->setMinimumHeight(40);
    centerLayout->addWidget(instructionHelpLabel_);

    splitter->addWidget(centerPanel);

    // 连接指令选择信号
    connect(disasmView_, &DisassemblerView::instructionSelected,
            this, &DebuggerWindow::updateInstructionHelp);

    splitter->setSizes({300, 600});

    setCentralWidget(splitter);

    statusBar()->showMessage(tr("Ready"));
}

void DebuggerWindow::onPauseToggled(bool checked) {
    paused_ = checked;
    if (pauseCallback_) {
        pauseCallback_(checked);
    }
    if (checked) {
        refresh();
        statusBar()->showMessage(tr("Paused"));
    } else {
        statusBar()->showMessage(tr("Running"));
    }
}

void DebuggerWindow::onStep() {
    if (!paused_ && pauseCallback_) {
        pauseCallback_(true);
        paused_ = true;
    }
    
    if (console_) {
        console_->getCPU().step();
        refresh();
    }
}

void DebuggerWindow::onStepOver() {
    // TODO: Implement proper step over (skip JSR)
    onStep();
}

void DebuggerWindow::onStepFrame() {
    if (console_) {
        console_->runFrame();
        refresh();
        statusBar()->showMessage(tr("Stepped 1 frame"));
    }
}

void DebuggerWindow::onReset() {
    if (console_) {
        console_->reset();
        refresh();
        statusBar()->showMessage(tr("Console reset"));
    }
}

void DebuggerWindow::onAutoRefresh() {
    // Only auto-refresh when not paused (to show live state)
    if (!paused_ && console_) {
        updateCPUStatus();
    }
}

void DebuggerWindow::onEmulationPaused() {
    paused_ = true;
    refresh();
}

void DebuggerWindow::syncBreakpoints(const BreakpointManager& bpm) {
    // Sync all breakpoints to the disassembly view
    // First clear existing breakpoints
    for (uint16_t existing : disasmView_->breakpoints()) {
        disasmView_->setBreakpoint(existing, false);
    }
    // Add all breakpoints from manager
    for (int i = 0; i < bpm.count(); i++) {
        const Breakpoint& bp = bpm.at(i);
        if (bp.isExec() && bp.isEnabled()) {
            disasmView_->setBreakpoint(bp.address, true);
        }
    }
}

void DebuggerWindow::setPauseAndGoto(uint16_t addr) {
    paused_ = true;
    if (pauseCallback_) {
        pauseCallback_(true);
    }
    // Update pause button state
    // Find the pause action in the toolbar
    for (QAction* act : findChildren<QAction*>()) {
        if (act->text() == tr("&Pause") || act->isCheckable()) {
            act->setChecked(true);
            break;
        }
    }
    refresh();
    statusBar()->showMessage(tr("Breakpoint hit at $%1").arg(addr, 4, 16, QLatin1Char('0')).toUpper());
}

void DebuggerWindow::refresh() {
    updateCPUStatus();
    if (console_) {
        uint16_t pc = console_->getCPU().PC;
        disasmView_->refresh(pc);
        updateInstructionHelp(pc);
    }
}

void DebuggerWindow::updateCPUStatus() {
    if (!console_) return;

    const CPU6502& cpu = console_->getCPU();
    
    pcLabel_->setText(QString("%1").arg(cpu.PC, 4, 16, QLatin1Char('0')).toUpper());
    aLabel_->setText(QString("%1").arg(cpu.A, 2, 16, QLatin1Char('0')).toUpper());
    xLabel_->setText(QString("%1").arg(cpu.X, 2, 16, QLatin1Char('0')).toUpper());
    yLabel_->setText(QString("%1").arg(cpu.Y, 2, 16, QLatin1Char('0')).toUpper());
    spLabel_->setText(QString("%1").arg(cpu.SP, 2, 16, QLatin1Char('0')).toUpper());
    
    // Status flags with color
    QString flags;
    auto flagSpan = [](bool set, char c) -> QString {
        if (set) {
            return QString("<span style='color:green;font-weight:bold;'>%1</span>").arg(c);
        }
        return QString("<span style='color:gray;'>%1</span>").arg(QChar(c).toLower());
    };
    flags = flagSpan(cpu.P & 0x80, 'N') + flagSpan(cpu.P & 0x40, 'V') + 
            QString("<span style='color:gray;'>-</span>") +
            flagSpan(cpu.P & 0x10, 'B') + flagSpan(cpu.P & 0x08, 'D') +
            flagSpan(cpu.P & 0x04, 'I') + flagSpan(cpu.P & 0x02, 'Z') +
            flagSpan(cpu.P & 0x01, 'C');
    statusLabel_->setText(flags);

    // PPU status
    const PPU& ppu = console_->getPPU();
    scanlineLabel_->setText(QString::number(ppu.scanline));
    cycleLabel_->setText(QString::number(ppu.dot));
}

void DebuggerWindow::updateDisassembly() {
    if (disasmView_ && console_) {
        disasmView_->refresh(console_->getCPU().PC);
    }
}

void DebuggerWindow::updateInstructionHelp(uint16_t addr) {
    if (!console_) return;

    uint8_t opcode = console_->getBus().mapper->cpuRead(addr);

    // 指令说明映射（简化版，只包含主要指令）
    static const QMap<uint8_t, QString> helpMap = {
        {0x69, tr("ADC - Add with Carry: A + M + C -> A, C")},
        {0x65, tr("ADC - Add with Carry: A + M + C -> A, C")},
        {0x75, tr("ADC - Add with Carry: A + M + C -> A, C")},
        {0x6D, tr("ADC - Add with Carry: A + M + C -> A, C")},
        {0x7D, tr("ADC - Add with Carry: A + M + C -> A, C")},
        {0x79, tr("ADC - Add with Carry: A + M + C -> A, C")},
        {0x61, tr("ADC - Add with Carry: A + M + C -> A, C")},
        {0x71, tr("ADC - Add with Carry: A + M + C -> A, C")},
        {0x29, tr("AND - Logical AND: A & M -> A")},
        {0x25, tr("AND - Logical AND: A & M -> A")},
        {0x35, tr("AND - Logical AND: A & M -> A")},
        {0x2D, tr("AND - Logical AND: A & M -> A")},
        {0x3D, tr("AND - Logical AND: A & M -> A")},
        {0x39, tr("AND - Logical AND: A & M -> A")},
        {0x21, tr("AND - Logical AND: A & M -> A")},
        {0x31, tr("AND - Logical AND: A & M -> A")},
        {0x0A, tr("ASL - Arithmetic Shift Left: A << 1 -> A, C")},
        {0x06, tr("ASL - Arithmetic Shift Left: M << 1 -> M, C")},
        {0x16, tr("ASL - Arithmetic Shift Left: M << 1 -> M, C")},
        {0x0E, tr("ASL - Arithmetic Shift Left: M << 1 -> M, C")},
        {0x1E, tr("ASL - Arithmetic Shift Left: M << 1 -> M, C")},
        {0x90, tr("BCC - Branch if Carry Clear: if C==0, PC + offset -> PC")},
        {0xB0, tr("BCS - Branch if Carry Set: if C==1, PC + offset -> PC")},
        {0xF0, tr("BEQ - Branch if Equal: if Z==1, PC + offset -> PC")},
        {0x30, tr("BMI - Branch if Minus: if N==1, PC + offset -> PC")},
        {0xD0, tr("BNE - Branch if Not Equal: if Z==0, PC + offset -> PC")},
        {0x10, tr("BPL - Branch if Plus: if N==0, PC + offset -> PC")},
        {0x50, tr("BVC - Branch if Overflow Clear: if V==0, PC + offset -> PC")},
        {0x70, tr("BVS - Branch if Overflow Set: if V==1, PC + offset -> PC")},
        {0x24, tr("BIT - Bit Test: A & M, set N,V,Z flags")},
        {0x2C, tr("BIT - Bit Test: A & M, set N,V,Z flags")},
        {0x00, tr("BRK - Break: Force Interrupt")},
        {0x18, tr("CLC - Clear Carry: 0 -> C")},
        {0xD8, tr("CLD - Clear Decimal: 0 -> D")},
        {0x58, tr("CLI - Clear Interrupt: 0 -> I")},
        {0xB8, tr("CLV - Clear Overflow: 0 -> V")},
        {0xC9, tr("CMP - Compare A: A - M, set flags")},
        {0xC5, tr("CMP - Compare A: A - M, set flags")},
        {0xD5, tr("CMP - Compare A: A - M, set flags")},
        {0xCD, tr("CMP - Compare A: A - M, set flags")},
        {0xDD, tr("CMP - Compare A: A - M, set flags")},
        {0xD9, tr("CMP - Compare A: A - M, set flags")},
        {0xC1, tr("CMP - Compare A: A - M, set flags")},
        {0xD1, tr("CMP - Compare A: A - M, set flags")},
        {0xE0, tr("CPX - Compare X: X - M, set flags")},
        {0xE4, tr("CPX - Compare X: X - M, set flags")},
        {0xEC, tr("CPX - Compare X: X - M, set flags")},
        {0xC0, tr("CPY - Compare Y: Y - M, set flags")},
        {0xC4, tr("CPY - Compare Y: Y - M, set flags")},
        {0xCC, tr("CPY - Compare Y: Y - M, set flags")},
        {0xC6, tr("DEC - Decrement: M - 1 -> M")},
        {0xD6, tr("DEC - Decrement: M - 1 -> M")},
        {0xCE, tr("DEC - Decrement: M - 1 -> M")},
        {0xDE, tr("DEC - Decrement: M - 1 -> M")},
        {0xCA, tr("DEX - Decrement X: X - 1 -> X")},
        {0x88, tr("DEY - Decrement Y: Y - 1 -> Y")},
        {0x49, tr("EOR - Exclusive OR: A ^ M -> A")},
        {0x45, tr("EOR - Exclusive OR: A ^ M -> A")},
        {0x55, tr("EOR - Exclusive OR: A ^ M -> A")},
        {0x4D, tr("EOR - Exclusive OR: A ^ M -> A")},
        {0x5D, tr("EOR - Exclusive OR: A ^ M -> A")},
        {0x59, tr("EOR - Exclusive OR: A ^ M -> A")},
        {0x41, tr("EOR - Exclusive OR: A ^ M -> A")},
        {0x51, tr("EOR - Exclusive OR: A ^ M -> A")},
        {0xE6, tr("INC - Increment: M + 1 -> M")},
        {0xF6, tr("INC - Increment: M + 1 -> M")},
        {0xEE, tr("INC - Increment: M + 1 -> M")},
        {0xFE, tr("INC - Increment: M + 1 -> M")},
        {0xE8, tr("INX - Increment X: X + 1 -> X")},
        {0xC8, tr("INY - Increment Y: Y + 1 -> Y")},
        {0x4C, tr("JMP - Jump: PC -> M")},
        {0x6C, tr("JMP - Jump Indirect: PC -> (M)")},
        {0x20, tr("JSR - Jump Subroutine: PC -> Stack, PC -> M")},
        {0xA9, tr("LDA - Load A: M -> A")},
        {0xA5, tr("LDA - Load A: M -> A")},
        {0xB5, tr("LDA - Load A: M -> A")},
        {0xAD, tr("LDA - Load A: M -> A")},
        {0xBD, tr("LDA - Load A: M -> A")},
        {0xB9, tr("LDA - Load A: M -> A")},
        {0xA1, tr("LDA - Load A: M -> A")},
        {0xB1, tr("LDA - Load A: M -> A")},
        {0xA2, tr("LDX - Load X: M -> X")},
        {0xA6, tr("LDX - Load X: M -> X")},
        {0xB6, tr("LDX - Load X: M -> X")},
        {0xAE, tr("LDX - Load X: M -> X")},
        {0xBE, tr("LDX - Load X: M -> X")},
        {0xA0, tr("LDY - Load Y: M -> Y")},
        {0xA4, tr("LDY - Load Y: M -> Y")},
        {0xB4, tr("LDY - Load Y: M -> Y")},
        {0xAC, tr("LDY - Load Y: M -> Y")},
        {0xBC, tr("LDY - Load Y: M -> Y")},
        {0x4A, tr("LSR - Logical Shift Right: A >> 1 -> A, C")},
        {0x46, tr("LSR - Logical Shift Right: M >> 1 -> M, C")},
        {0x56, tr("LSR - Logical Shift Right: M >> 1 -> M, C")},
        {0x4E, tr("LSR - Logical Shift Right: M >> 1 -> M, C")},
        {0x5E, tr("LSR - Logical Shift Right: M >> 1 -> M, C")},
        {0xEA, tr("NOP - No Operation")},
        {0x09, tr("ORA - Logical OR: A | M -> A")},
        {0x05, tr("ORA - Logical OR: A | M -> A")},
        {0x15, tr("ORA - Logical OR: A | M -> A")},
        {0x0D, tr("ORA - Logical OR: A | M -> A")},
        {0x1D, tr("ORA - Logical OR: A | M -> A")},
        {0x19, tr("ORA - Logical OR: A | M -> A")},
        {0x01, tr("ORA - Logical OR: A | M -> A")},
        {0x11, tr("ORA - Logical OR: A | M -> A")},
        {0x48, tr("PHA - Push A: A -> Stack")},
        {0x08, tr("PHP - Push P: P -> Stack")},
        {0x68, tr("PLA - Pull A: Stack -> A")},
        {0x28, tr("PLP - Pull P: Stack -> P")},
        {0x2A, tr("ROL - Rotate Left: A << 1 + C -> A")},
        {0x26, tr("ROL - Rotate Left: M << 1 + C -> M")},
        {0x36, tr("ROL - Rotate Left: M << 1 + C -> M")},
        {0x2E, tr("ROL - Rotate Left: M << 1 + C -> M")},
        {0x3E, tr("ROL - Rotate Left: M << 1 + C -> M")},
        {0x6A, tr("ROR - Rotate Right: A >> 1 + C -> A")},
        {0x66, tr("ROR - Rotate Right: M >> 1 + C -> M")},
        {0x76, tr("ROR - Rotate Right: M >> 1 + C -> M")},
        {0x6E, tr("ROR - Rotate Right: M >> 1 + C -> M")},
        {0x7E, tr("ROR - Rotate Right: M >> 1 + C -> M")},
        {0x40, tr("RTI - Return from Interrupt: Stack -> PC, P")},
        {0x60, tr("RTS - Return from Subroutine: Stack -> PC")},
        {0xE9, tr("SBC - Subtract with Carry: A - M - ~C -> A")},
        {0xE5, tr("SBC - Subtract with Carry: A - M - ~C -> A")},
        {0xF5, tr("SBC - Subtract with Carry: A - M - ~C -> A")},
        {0xED, tr("SBC - Subtract with Carry: A - M - ~C -> A")},
        {0xFD, tr("SBC - Subtract with Carry: A - M - ~C -> A")},
        {0xF9, tr("SBC - Subtract with Carry: A - M - ~C -> A")},
        {0xE1, tr("SBC - Subtract with Carry: A - M - ~C -> A")},
        {0xF1, tr("SBC - Subtract with Carry: A - M - ~C -> A")},
        {0x38, tr("SEC - Set Carry: 1 -> C")},
        {0xF8, tr("SED - Set Decimal: 1 -> D")},
        {0x78, tr("SEI - Set Interrupt: 1 -> I")},
        {0x85, tr("STA - Store A: A -> M")},
        {0x95, tr("STA - Store A: A -> M")},
        {0x8D, tr("STA - Store A: A -> M")},
        {0x9D, tr("STA - Store A: A -> M")},
        {0x99, tr("STA - Store A: A -> M")},
        {0x81, tr("STA - Store A: A -> M")},
        {0x91, tr("STA - Store A: A -> M")},
        {0x86, tr("STX - Store X: X -> M")},
        {0x96, tr("STX - Store X: X -> M")},
        {0x8E, tr("STX - Store X: X -> M")},
        {0x84, tr("STY - Store Y: Y -> M")},
        {0x94, tr("STY - Store Y: Y -> M")},
        {0x8C, tr("STY - Store Y: Y -> M")},
        {0xAA, tr("TAX - Transfer A to X: A -> X")},
        {0xA8, tr("TAY - Transfer A to Y: A -> Y")},
        {0xBA, tr("TSX - Transfer SP to X: SP -> X")},
        {0x8A, tr("TXA - Transfer X to A: X -> A")},
        {0x9A, tr("TXS - Transfer X to SP: X -> SP")},
        {0x98, tr("TYA - Transfer Y to A: Y -> A")},
    };

    QString help = helpMap.value(opcode, tr("Unknown instruction"));
    instructionHelpLabel_->setText(help);
}

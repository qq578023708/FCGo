#include "disassembler.h"
#include "../../core/nes_console.h"
#include "../../core/cpu/cpu6502.h"

#include <QHeaderView>
#include <QFont>

// ---------------------------------------------------------------------------
// Color palette (OllyDbg-inspired, foreground only, no background)
// ---------------------------------------------------------------------------
static const QColor kColorJump(0, 100, 255);       // blue   - branch/jump
static const QColor kColorLoad(0, 180, 0);         // green  - load
static const QColor kColorStore(255, 140, 0);      // orange - store
static const QColor kColorStack(180, 0, 255);      // purple - stack
static const QColor kColorALU(0, 180, 180);        // cyan   - arithmetic/logic
static const QColor kColorCompare(255, 255, 0);    // yellow - compare/test
static const QColor kColorFlag(160, 160, 160);     // gray   - flag
static const QColor kColorInvalid(100, 100, 100);  // dark gray - invalid
static const QColor kColorPC(255, 255, 255);       // white  - PC row text
static const QColor kColorBP(255, 80, 80);         // red    - breakpoint dot

// ---------------------------------------------------------------------------
// Complete 6502 opcode table
// ---------------------------------------------------------------------------
struct OpcodeInfo {
    const char* name;
    int size;
    const char* mode;
};

// clang-format off
static const OpcodeInfo opcodeTable[256] = {
    {"BRK",1,"imp"}, {"ORA",2,"inx"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"ORA",2,"zpg"}, {"ASL",2,"zpg"}, {"???",1,"imp"},
    {"PHP",1,"imp"}, {"ORA",2,"imm"}, {"ASL",1,"acc"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"ORA",3,"abs"}, {"ASL",3,"abs"}, {"???",1,"imp"},
    {"BPL",2,"rel"}, {"ORA",2,"iny"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"ORA",2,"zpx"}, {"ASL",2,"zpx"}, {"???",1,"imp"},
    {"CLC",1,"imp"}, {"ORA",3,"aby"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"ORA",3,"abx"}, {"ASL",3,"abx"}, {"???",1,"imp"},
    {"JSR",3,"abs"}, {"AND",2,"inx"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"BIT",2,"zpg"}, {"AND",2,"zpg"}, {"ROL",2,"zpg"}, {"???",1,"imp"},
    {"PLP",1,"imp"}, {"AND",2,"imm"}, {"ROL",1,"acc"}, {"???",1,"imp"},
    {"BIT",3,"abs"}, {"AND",3,"abs"}, {"ROL",3,"abs"}, {"???",1,"imp"},
    {"BMI",2,"rel"}, {"AND",2,"iny"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"AND",2,"zpx"}, {"ROL",2,"zpx"}, {"???",1,"imp"},
    {"SEC",1,"imp"}, {"AND",3,"aby"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"AND",3,"abx"}, {"ROL",3,"abx"}, {"???",1,"imp"},
    {"RTI",1,"imp"}, {"EOR",2,"inx"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"EOR",2,"zpg"}, {"LSR",2,"zpg"}, {"???",1,"imp"},
    {"PHA",1,"imp"}, {"EOR",2,"imm"}, {"LSR",1,"acc"}, {"???",1,"imp"},
    {"JMP",3,"abs"}, {"EOR",3,"abs"}, {"LSR",3,"abs"}, {"???",1,"imp"},
    {"BVC",2,"rel"}, {"EOR",2,"iny"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"EOR",2,"zpx"}, {"LSR",2,"zpx"}, {"???",1,"imp"},
    {"CLI",1,"imp"}, {"EOR",3,"aby"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"EOR",3,"abx"}, {"LSR",3,"abx"}, {"???",1,"imp"},
    {"RTS",1,"imp"}, {"ADC",2,"inx"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"ADC",2,"zpg"}, {"ROR",2,"zpg"}, {"???",1,"imp"},
    {"PLA",1,"imp"}, {"ADC",2,"imm"}, {"ROR",1,"acc"}, {"???",1,"imp"},
    {"JMP",3,"ind"}, {"ADC",3,"abs"}, {"ROR",3,"abs"}, {"???",1,"imp"},
    {"BVS",2,"rel"}, {"ADC",2,"iny"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"ADC",2,"zpx"}, {"ROR",2,"zpx"}, {"???",1,"imp"},
    {"SEI",1,"imp"}, {"ADC",3,"aby"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"ADC",3,"abx"}, {"ROR",3,"abx"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"STA",2,"inx"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"STY",2,"zpg"}, {"STA",2,"zpg"}, {"STX",2,"zpg"}, {"???",1,"imp"},
    {"DEY",1,"imp"}, {"???",1,"imp"}, {"TXA",1,"imp"}, {"???",1,"imp"},
    {"STY",3,"abs"}, {"STA",3,"abs"}, {"STX",3,"abs"}, {"???",1,"imp"},
    {"BCC",2,"rel"}, {"STA",2,"iny"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"STY",2,"zpx"}, {"STA",2,"zpx"}, {"STX",2,"zpy"}, {"???",1,"imp"},
    {"TYA",1,"imp"}, {"STA",3,"aby"}, {"TXS",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"STA",3,"abx"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"LDY",2,"imm"}, {"LDA",2,"inx"}, {"LDX",2,"imm"}, {"???",1,"imp"},
    {"LDY",2,"zpg"}, {"LDA",2,"zpg"}, {"LDX",2,"zpg"}, {"???",1,"imp"},
    {"TAY",1,"imp"}, {"LDA",2,"imm"}, {"TAX",1,"imp"}, {"???",1,"imp"},
    {"LDY",3,"abs"}, {"LDA",3,"abs"}, {"LDX",3,"abs"}, {"???",1,"imp"},
    {"BCS",2,"rel"}, {"LDA",2,"iny"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"LDY",2,"zpx"}, {"LDA",2,"zpx"}, {"LDX",2,"zpy"}, {"???",1,"imp"},
    {"CLV",1,"imp"}, {"LDA",3,"aby"}, {"TSX",1,"imp"}, {"???",1,"imp"},
    {"LDY",3,"abx"}, {"LDA",3,"abx"}, {"LDX",3,"aby"}, {"???",1,"imp"},
    {"CPY",2,"imm"}, {"CMP",2,"inx"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"CPY",2,"zpg"}, {"CMP",2,"zpg"}, {"DEC",2,"zpg"}, {"???",1,"imp"},
    {"INY",1,"imp"}, {"CMP",2,"imm"}, {"DEX",1,"imp"}, {"???",1,"imp"},
    {"CPY",3,"abs"}, {"CMP",3,"abs"}, {"DEC",3,"abs"}, {"???",1,"imp"},
    {"BNE",2,"rel"}, {"CMP",2,"iny"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"CMP",2,"zpx"}, {"DEC",2,"zpx"}, {"???",1,"imp"},
    {"CLD",1,"imp"}, {"CMP",3,"aby"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"CMP",3,"abx"}, {"DEC",3,"abx"}, {"???",1,"imp"},
    {"CPX",2,"imm"}, {"SBC",2,"inx"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"CPX",2,"zpg"}, {"SBC",2,"zpg"}, {"INC",2,"zpg"}, {"???",1,"imp"},
    {"INX",1,"imp"}, {"SBC",2,"imm"}, {"NOP",1,"imp"}, {"???",1,"imp"},
    {"CPX",3,"abs"}, {"SBC",3,"abs"}, {"INC",3,"abs"}, {"???",1,"imp"},
    {"BEQ",2,"rel"}, {"SBC",2,"iny"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"SBC",2,"zpx"}, {"INC",2,"zpx"}, {"???",1,"imp"},
    {"SED",1,"imp"}, {"SBC",3,"aby"}, {"???",1,"imp"}, {"???",1,"imp"},
    {"???",1,"imp"}, {"SBC",3,"abx"}, {"INC",3,"abx"}, {"???",1,"imp"},
};
// clang-format on

// ===========================================================================
// DisasmModel
// ===========================================================================

DisasmModel::DisasmModel(NESConsole* console, QObject* parent)
    : QAbstractTableModel(parent)
    , console_(console)
{
}

int DisasmModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(lines_.size());
}

int DisasmModel::columnCount(const QModelIndex&) const
{
    return 4; // BP | Address | Bytes | Instruction
}

QVariant DisasmModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(lines_.size()))
        return {};

    const DisasmLine& line = lines_[index.row()];
    int col = index.column();

    if (role == Qt::DisplayRole) {
        switch (col) {
        case 0: {
            QString indicators;
            if (line.addr == currentPC_)
                indicators += QString::fromUtf8("\u25B6 ");
            if (breakpoints_.contains(line.addr))
                indicators += QString::fromUtf8("\u25CF");
            return indicators;
        }
        case 1: return QString("$%1").arg(line.addr, 4, 16, QLatin1Char('0')).toUpper();
        case 2: return line.bytes;
        case 3: return line.instruction;
        default: return {};
        }
    }

    if (role == Qt::TextAlignmentRole) {
        return col == 0 ? QVariant(Qt::AlignCenter)
                         : QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    }

    if (role == Qt::FontRole) {
        static QFont mono("Consolas", 11);  // Larger font for better readability
        mono.setStyleHint(QFont::Monospace);
        return mono;
    }

    // Foreground only — no background colors
    if (role == Qt::ForegroundRole) {
        if (line.addr == currentPC_)
            return kColorPC;
        if (col == 0 && breakpoints_.contains(line.addr))
            return kColorBP;
        if (col == 3) {
            const QString& cat = line.category;
            if (cat == "jump")    return kColorJump;
            if (cat == "load")    return kColorLoad;
            if (cat == "store")   return kColorStore;
            if (cat == "stack")   return kColorStack;
            if (cat == "alu")     return kColorALU;
            if (cat == "compare") return kColorCompare;
            if (cat == "flag")    return kColorFlag;
            if (cat == "invalid") return kColorInvalid;
        }
        // Brighter color for bytes column (col 2)
        if (col == 2) return QColor(180, 220, 255);  // Light blue for bytes
        return QColor(220, 220, 220);
    }

    return {};
}

QVariant DisasmModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case 0: return tr("BP");
    case 1: return tr("Address");
    case 2: return tr("Bytes");
    case 3: return tr("Instruction");
    default: return {};
    }
}

// ---------------------------------------------------------------------------
// rebuild() — precompute all visible lines (called once, ~0.1ms)
// ---------------------------------------------------------------------------
void DisasmModel::rebuild(uint16_t centerAddr)
{
    beginResetModel();
    lines_.clear();
    currentPC_ = centerAddr;

    // Go back ~30 instructions to center the view
    uint16_t addr = centerAddr;
    for (int i = 0; i < 30; i++) {
        if (addr < 3) break;
        addr -= 3;
    }
    // Align to instruction boundary
    uint16_t scan = addr;
    while (scan < centerAddr) {
        DisasmLine dl;
        int bytes = disassembleOne(scan, dl);
        scan += bytes;
        if (scan > centerAddr) { addr = scan - bytes; break; }
    }
    if (scan <= centerAddr) addr = scan;

    // Disassemble forward
    for (int i = 0; i < kMaxLines; i++) {
        if (addr > 0xFFFF) break;
        DisasmLine dl;
        int bytes = disassembleOne(addr, dl);
        lines_.push_back(std::move(dl));
        addr += bytes;
        if (bytes == 0) addr++; // guard
    }

    endResetModel();
}

// ---------------------------------------------------------------------------
// setPC() — fast highlight update, no rebuild
// ---------------------------------------------------------------------------
void DisasmModel::setPC(uint16_t pc)
{
    uint16_t oldPC = currentPC_;
    currentPC_ = pc;

    int oldRow = rowForAddress(oldPC);
    int newRow = rowForAddress(pc);

    if (oldRow >= 0)
        emit dataChanged(index(oldRow, 0), index(oldRow, columnCount() - 1),
                        {Qt::DisplayRole, Qt::ForegroundRole});
    if (newRow >= 0)
        emit dataChanged(index(newRow, 0), index(newRow, columnCount() - 1),
                        {Qt::DisplayRole, Qt::ForegroundRole});
}

void DisasmModel::setBreakpoint(uint16_t addr, bool enabled)
{
    if (enabled) breakpoints_.insert(addr);
    else         breakpoints_.remove(addr);

    int row = rowForAddress(addr);
    if (row >= 0)
        emit dataChanged(index(row, 0), index(row, columnCount() - 1),
                         {Qt::DisplayRole, Qt::ForegroundRole});
}

bool DisasmModel::hasBreakpoint(uint16_t addr) const
{
    return breakpoints_.contains(addr);
}

uint16_t DisasmModel::addressAtRow(int row) const
{
    if (row >= 0 && row < static_cast<int>(lines_.size()))
        return lines_[row].addr;
    return 0;
}

int DisasmModel::rowForAddress(uint16_t addr) const
{
    // Binary search on precomputed addresses
    int lo = 0, hi = static_cast<int>(lines_.size()) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint16_t midAddr = lines_[mid].addr;
        if (midAddr == addr) return mid;
        if (midAddr < addr) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// disassembleOne() — disassemble a single instruction
// ---------------------------------------------------------------------------
int DisasmModel::disassembleOne(uint16_t addr, DisasmLine& out) const
{
    uint8_t opcode = readByte(addr);
    uint8_t lo = readByte(addr + 1);
    uint8_t hi = readByte(addr + 2);
    const OpcodeInfo& info = opcodeTable[opcode];
    int bytes = info.size;

    QString bytesStr;
    for (int b = 0; b < bytes && b < 3; ++b) {
        if (b > 0) bytesStr += ' ';
        bytesStr += QString("%1").arg(readByte(addr + b), 2, 16, QLatin1Char('0')).toUpper();
    }

    QString instrStr;
    switch (bytes) {
    case 1: instrStr = QString(info.name); break;
    case 2: {
        QString m(info.mode);
        if (m == "rel")      instrStr = QString("%1 $%2").arg(info.name).arg((uint16_t)(addr+2+(int8_t)lo), 4, 16, QLatin1Char('0')).toUpper();
        else if (m == "imm") instrStr = QString("%1 #$%2").arg(info.name).arg(lo, 2, 16, QLatin1Char('0')).toUpper();
        else if (m == "zpg") instrStr = QString("%1 $%2").arg(info.name).arg(lo, 2, 16, QLatin1Char('0')).toUpper();
        else if (m == "zpx") instrStr = QString("%1 $%2,X").arg(info.name).arg(lo, 2, 16, QLatin1Char('0')).toUpper();
        else if (m == "zpy") instrStr = QString("%1 $%2,Y").arg(info.name).arg(lo, 2, 16, QLatin1Char('0')).toUpper();
        else if (m == "inx") instrStr = QString("%1 ($%2,X)").arg(info.name).arg(lo, 2, 16, QLatin1Char('0')).toUpper();
        else if (m == "iny") instrStr = QString("%1 ($%2),Y").arg(info.name).arg(lo, 2, 16, QLatin1Char('0')).toUpper();
        else                 instrStr = QString("%1 $%2").arg(info.name).arg(lo, 2, 16, QLatin1Char('0')).toUpper();
        break;
    }
    case 3: {
        QString m(info.mode);
        uint16_t a16 = (hi << 8) | lo;
        if (m == "abs") instrStr = QString("%1 $%2").arg(info.name).arg(a16, 4, 16, QLatin1Char('0')).toUpper();
        else if (m == "abx") instrStr = QString("%1 $%2,X").arg(info.name).arg(a16, 4, 16, QLatin1Char('0')).toUpper();
        else if (m == "aby") instrStr = QString("%1 $%2,Y").arg(info.name).arg(a16, 4, 16, QLatin1Char('0')).toUpper();
        else if (m == "ind") instrStr = QString("%1 ($%2)").arg(info.name).arg(a16, 4, 16, QLatin1Char('0')).toUpper();
        else                 instrStr = QString("%1 $%2").arg(info.name).arg(a16, 4, 16, QLatin1Char('0')).toUpper();
        break;
    }
    default: instrStr = "???"; break;
    }

    out.addr = addr;
    out.bytes = bytesStr;
    out.instruction = instrStr;
    out.category = getCategory(info.name);
    return bytes;
}

uint8_t DisasmModel::readByte(uint16_t addr) const
{
    if (console_ && console_->getBus().mapper)
        return console_->getBus().mapper->cpuRead(addr);
    return 0;
}

QString DisasmModel::getCategory(const char* n) const
{
    if (!strcmp(n,"BPL")||!strcmp(n,"BMI")||!strcmp(n,"BVC")||!strcmp(n,"BVS")||
        !strcmp(n,"BCC")||!strcmp(n,"BCS")||!strcmp(n,"BNE")||!strcmp(n,"BEQ")||
        !strcmp(n,"JMP")||!strcmp(n,"JSR")||!strcmp(n,"RTS")||!strcmp(n,"RTI")||
        !strcmp(n,"BRK")) return "jump";
    if (!strcmp(n,"LDA")||!strcmp(n,"LDX")||!strcmp(n,"LDY")) return "load";
    if (!strcmp(n,"STA")||!strcmp(n,"STX")||!strcmp(n,"STY")) return "store";
    if (!strcmp(n,"PHA")||!strcmp(n,"PHP")||!strcmp(n,"PLA")||!strcmp(n,"PLP")||
        !strcmp(n,"TXS")||!strcmp(n,"TSX")) return "stack";
    if (!strcmp(n,"ADC")||!strcmp(n,"SBC")||!strcmp(n,"AND")||!strcmp(n,"ORA")||
        !strcmp(n,"EOR")||!strcmp(n,"INC")||!strcmp(n,"DEC")||!strcmp(n,"INX")||
        !strcmp(n,"INY")||!strcmp(n,"DEX")||!strcmp(n,"DEY")||!strcmp(n,"ASL")||
        !strcmp(n,"LSR")||!strcmp(n,"ROL")||!strcmp(n,"ROR")) return "alu";
    if (!strcmp(n,"CMP")||!strcmp(n,"CPX")||!strcmp(n,"CPY")||!strcmp(n,"BIT")) return "compare";
    if (!strcmp(n,"CLC")||!strcmp(n,"SEC")||!strcmp(n,"CLI")||!strcmp(n,"SEI")||
        !strcmp(n,"CLD")||!strcmp(n,"SED")||!strcmp(n,"CLV")||!strcmp(n,"NOP")) return "flag";
    return "invalid";
}

// ===========================================================================
// DisassemblerView
// ===========================================================================

DisassemblerView::DisassemblerView(NESConsole* console, QWidget* parent)
    : QTableView(parent)
    , console_(console)
{
    setupUI();
}

void DisassemblerView::setupUI()
{
    model_ = new DisasmModel(console_, this);
    setModel(model_);

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->hide();  // cleaner look

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(this, &QTableView::doubleClicked, this, &DisassemblerView::onDoubleClicked);
    connect(selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex& cur, const QModelIndex&) {
                if (cur.isValid()) emit instructionSelected(model_->addressAtRow(cur.row()));
            });
}

void DisassemblerView::refresh(uint16_t pc)
{
    // Only rebuild if PC is far from visible range
    int pcRow = model_->rowForAddress(pc);
    if (pcRow < 0) {
        model_->rebuild(pc);
        pcRow = model_->rowForAddress(pc);
    }
    model_->setPC(pc);

    if (pcRow >= 0) {
        scrollTo(model_->index(pcRow, 0), QAbstractItemView::PositionAtCenter);
        selectRow(pcRow);
    }
}

void DisassemblerView::setBreakpoint(uint16_t addr, bool enabled)
{
    model_->setBreakpoint(addr, enabled);
}

const QSet<uint16_t>& DisassemblerView::breakpoints() const
{
    return model_->breakpoints();
}

void DisassemblerView::onDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    uint16_t addr = model_->addressAtRow(index.row());
    if (index.column() == 0) {
        setBreakpoint(addr, !model_->hasBreakpoint(addr));
    }
}

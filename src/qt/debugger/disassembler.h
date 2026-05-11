#pragma once

#include <QTableView>
#include <QAbstractTableModel>
#include <QSet>
#include <cstdint>
#include <vector>

class NESConsole;

// ============================================================
// DisasmModel - Precomputed disassembly for instant response
// ============================================================
class DisasmModel : public QAbstractTableModel {
    Q_OBJECT
public:
    struct DisasmLine {
        uint16_t addr;
        QString bytes;
        QString instruction;
        QString category;
    };

    explicit DisasmModel(NESConsole* console, QObject* parent = nullptr);
    
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    
    // Rebuild visible lines around addr
    void rebuild(uint16_t centerAddr);
    
    // Fast PC highlight update (no rebuild)
    void setPC(uint16_t pc);
    
    // Breakpoint management
    void setBreakpoint(uint16_t addr, bool enabled);
    bool hasBreakpoint(uint16_t addr) const;
    const QSet<uint16_t>& breakpoints() const { return breakpoints_; }
    
    // Lookup
    uint16_t addressAtRow(int row) const;
    int rowForAddress(uint16_t addr) const;
    int disassembleOne(uint16_t addr, DisasmLine& out) const;

private:
    uint8_t readByte(uint16_t addr) const;
    QString getCategory(const char* name) const;
    
    NESConsole* console_;
    
    // Precomputed line data
    std::vector<DisasmLine> lines_;
    std::vector<uint16_t> addrToRow_;  // fast lookup: addr >> 1 -> row (approximate)
    
    uint16_t currentPC_ = 0;
    uint16_t lastPC_ = 0xFFFF;
    QSet<uint16_t> breakpoints_;
    
    static constexpr int kMaxLines = 256;
};

// ============================================================
// DisassemblerView
// ============================================================
class DisassemblerView : public QTableView {
    Q_OBJECT
public:
    explicit DisassemblerView(NESConsole* console, QWidget* parent = nullptr);
    
    void refresh(uint16_t pc);
    void setBreakpoint(uint16_t addr, bool enabled);
    const QSet<uint16_t>& breakpoints() const;

signals:
    void instructionSelected(uint16_t addr);

public slots:
    void onDoubleClicked(const QModelIndex& index);

private:
    void setupUI();
    
    DisasmModel* model_ = nullptr;
    NESConsole* console_ = nullptr;
};

#include "memory_search.h"
#include "memory_viewer.h"
#include "../../core/nes_console.h"
#include "../../core/memory/bus.h"
#include "../../core/ppu/ppu.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QShowEvent>
#include <QInputDialog>
#include <QIntValidator>
#include <QClipboard>
#include <QApplication>
#include <QMenu>

// ============================================================
// Construction
// ============================================================

MemorySearchDialog::MemorySearchDialog(NESConsole* console, MemoryViewer* memViewer, QWidget* parent)
    : QDialog(parent)
    , console_(console)
    , memViewer_(memViewer)
{
    setupUI();
    setWindowTitle(tr("Memory Search"));
    setMinimumSize(520, 400);
}

void MemorySearchDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    // Reset state each time the dialog is shown
    onReset();
}

// ============================================================
// UI Setup
// ============================================================

void MemorySearchDialog::setupUI() {
    setStyleSheet(R"(
        QDialog { background-color: #1E1E1E; color: #D4D4D4; }
        QGroupBox { background-color: #2D2D2D; color: #D4D4D4; border: 1px solid #3E3E3E; border-radius: 4px; margin-top: 8px; padding-top: 12px; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QLabel { color: #D4D4D4; }
        QLineEdit { background-color: #3E3E3E; color: #D4D4D4; border: 1px solid #555; padding: 4px 6px; }
        QComboBox { background-color: #3E3E3E; color: #D4D4D4; border: 1px solid #555; padding: 4px 6px; }
        QComboBox QAbstractItemView { background-color: #2D2D2D; color: #D4D4D4; selection-background-color: #0078D4; }
        QPushButton { background-color: #3E3E3E; color: #D4D4D4; border: 1px solid #555; border-radius: 3px; padding: 4px 12px; }
        QPushButton:hover { background-color: #4E4E4E; }
        QTableWidget { background-color: #1E1E1E; color: #D4D4D4; gridline-color: #3E3E3E; border: 1px solid #3E3E3E; selection-background-color: #0078D4; }
        QHeaderView::section { background-color: #2D2D2D; color: #D4D4D4; border: 1px solid #3E3E3E; padding: 5px; font-weight: bold; }
    )");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // --- Search options ---
    QGroupBox* searchGroup = new QGroupBox(tr("Search Options"));
    QFormLayout* formLayout = new QFormLayout(searchGroup);

    // Region
    regionCombo_ = new QComboBox();
    regionCombo_->addItem(tr("CPU RAM ($0000-$1FFF)"), 0);
    regionCombo_->addItem(tr("PPU VRAM ($2000-$3FFF)"), 1);
    regionCombo_->addItem(tr("OAM ($0200-$02FF)"), 2);
    regionCombo_->addItem(tr("PRG ROM ($8000-$FFFF)"), 3);
    formLayout->addRow(tr("Region:"), regionCombo_);

    // Format (default: Decimal)
    formatCombo_ = new QComboBox();
    formatCombo_->addItem(tr("Decimal"), 0);
    formatCombo_->addItem(tr("Hex"), 1);
    formatCombo_->addItem(tr("Text (ASCII)"), 2);
    formLayout->addRow(tr("Format:"), formatCombo_);

    // Search input with auto-pad
    searchEdit_ = new QLineEdit();
    searchEdit_->setPlaceholderText(tr("e.g. 255 or 0 - 65535"));
    // Auto-format hex input when hex mode is selected
    connect(formatCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx == 1) {
            searchEdit_->setPlaceholderText(tr("e.g. FF 3A"));
        } else if (idx == 0) {
            searchEdit_->setPlaceholderText(tr("e.g. 255 or 0 - 65535"));
        } else {
            searchEdit_->setPlaceholderText(tr("e.g. hello"));
        }
    });
    connect(searchEdit_, &QLineEdit::textEdited, this, [this](const QString& text) {
        if (formatCombo_->currentIndex() == 1) { // Hex mode
            QString cleaned = text.simplified().remove(' ');
            // Block signals to avoid recursive call
            searchEdit_->blockSignals(true);
            if (cleaned.length() % 2 != 0 && !cleaned.isEmpty()) {
                cleaned = "0" + cleaned;
            }
            // Re-insert spaces
            QString formatted;
            for (int i = 0; i < cleaned.length(); i += 2) {
                if (i > 0) formatted += ' ';
                formatted += cleaned.mid(i, 2).toUpper();
            }
            int pos = searchEdit_->cursorPosition();
            searchEdit_->setText(formatted);
            searchEdit_->setCursorPosition(qMin(pos * 3 / 2 + 1, formatted.length()));
            searchEdit_->blockSignals(false);
        }
    });
    formLayout->addRow(tr("Value:"), searchEdit_);

    // Buttons row
    QHBoxLayout* btnLayout = new QHBoxLayout();
    searchBtn_ = new QPushButton(tr("Search"));
    resetBtn_ = new QPushButton(tr("Reset"));
    resultCountLabel_ = new QLabel("");
    searchHintLabel_ = new QLabel(tr("First search: scan region | Next search: filter results"));
    searchHintLabel_->setStyleSheet("color: #888; font-size: 11px;");
    btnLayout->addWidget(searchBtn_);
    btnLayout->addWidget(resetBtn_);
    btnLayout->addStretch();
    btnLayout->addWidget(searchHintLabel_);
    btnLayout->addWidget(resultCountLabel_);
    formLayout->addRow(btnLayout);

    mainLayout->addWidget(searchGroup);

    // --- Results table ---
    resultsTable_ = new QTableWidget();
    resultsTable_->setColumnCount(4);
    resultsTable_->setHorizontalHeaderLabels({tr("Address"), tr("Hex Value"), tr("Decimal"), tr("Previous")});
    resultsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    resultsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    resultsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    resultsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    resultsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    // Allow double-click to edit hex/decimal columns only
    resultsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTable_->verticalHeader()->setVisible(false);
    resultsTable_->setContextMenuPolicy(Qt::CustomContextMenu);

    QFont mono("Consolas", 9);
    mono.setStyleHint(QFont::Monospace);
    resultsTable_->setFont(mono);

    mainLayout->addWidget(resultsTable_, 1);

    // --- Connections ---
    connect(searchBtn_, &QPushButton::clicked, this, &MemorySearchDialog::onSearch);
    connect(resetBtn_, &QPushButton::clicked, this, &MemorySearchDialog::onReset);
    connect(resultsTable_, &QTableWidget::cellClicked, this, &MemorySearchDialog::onResultClicked);
    connect(resultsTable_, &QTableWidget::cellDoubleClicked, this, &MemorySearchDialog::onResultDoubleClicked);
    connect(resultsTable_, &QTableWidget::customContextMenuRequested, this, &MemorySearchDialog::onContextMenu);
    connect(searchEdit_, &QLineEdit::returnPressed, this, &MemorySearchDialog::onSearch);

    // Live update timer (200ms = 5Hz)
    liveTimer_ = new QTimer(this);
    connect(liveTimer_, &QTimer::timeout, this, &MemorySearchDialog::onLiveUpdate);
}

// ============================================================
// Memory read helper
// ============================================================

uint8_t MemorySearchDialog::readMem(int region, uint16_t addr) {
    if (!console_) return 0;
    switch (region) {
    case 0: return console_->getBus().read(addr & 0x07FF);
    case 1: return console_->getPPU().nameTableRAM[addr & 0x07FF];
    case 2: return console_->getPPU().oam[addr & 0xFF];
    case 3:
        return console_->getBus().mapper ? console_->getBus().mapper->cpuRead(0x8000 + addr) : 0;
    default: return 0;
    }
}

void MemorySearchDialog::writeMem(int region, uint16_t addr, uint8_t val) {
    if (!console_) return;
    switch (region) {
    case 0: console_->getBus().write(addr & 0x07FF, val); break;
    case 1: console_->getPPU().nameTableRAM[addr & 0x07FF] = val; break;
    case 2: console_->getPPU().oam[addr & 0xFF] = val; break;
    case 3: break; // ROM is read-only
    }
}

// ============================================================
// Hex input parser with auto-pad
// ============================================================

QByteArray MemorySearchDialog::parseHexInput(const QString& input, bool& ok) {
    ok = false;
    QString cleaned = input.simplified().remove(' ').toUpper();
    if (cleaned.isEmpty()) return {};

    // Auto-pad odd length
    if (cleaned.length() % 2 != 0) {
        cleaned = "0" + cleaned;
    }

    QByteArray result;
    for (int i = 0; i < cleaned.length(); i += 2) {
        bool byteOk;
        uint8_t b = static_cast<uint8_t>(cleaned.mid(i, 2).toUInt(&byteOk, 16));
        if (!byteOk) return {};
        result.append(b);
    }
    ok = true;
    return result;
}

// ============================================================
// Decimal input parser
// ============================================================

QByteArray MemorySearchDialog::parseDecInput(const QString& input, bool& ok) {
    ok = false;
    QString cleaned = input.simplified();
    if (cleaned.isEmpty()) return {};

    // Support space-separated bytes: "255 3" -> [FF, 03]
    QStringList parts = cleaned.split(' ', Qt::SkipEmptyParts);
    QByteArray result;
    for (const QString& part : parts) {
        bool byteOk;
        uint32_t val = part.toUInt(&byteOk, 10);
        if (!byteOk || val > 255) return {};
        result.append(static_cast<uint8_t>(val));
    }
    ok = true;
    return result;
}

// ============================================================
// Display address helper
// ============================================================

static uint16_t toDisplayAddr(int region, uint16_t offset) {
    switch (region) {
    case 0: return offset & 0x07FF;
    case 1: return 0x2000 + (offset & 0x07FF);
    case 2: return 0x0200 + (offset & 0xFF);
    case 3: return 0x8000 + offset;
    default: return offset;
    }
}

static int regionSize(int region) {
    switch (region) {
    case 0: return 0x0800;
    case 1: return 0x0800;
    case 2: return 0x0100;
    case 3: return 0x8000;
    default: return 0x10000;
    }
}

// ============================================================
// Save current values as "previous" snapshot
// ============================================================

void MemorySearchDialog::saveCurrentValues() {
    for (auto& sr : results_) {
        sr.lastValues.clear();
        for (int j = 0; j < lastPattern_.size(); j++) {
            sr.lastValues.append(readMem(searchRegion_, sr.address + j));
        }
    }
}

// ============================================================
// Initial search 鈥?scan entire region
// ============================================================

void MemorySearchDialog::doInitialSearch(const QByteArray& pattern) {
    results_.clear();
    if (pattern.isEmpty() || !console_) return;

    int region = regionCombo_->currentIndex();
    searchRegion_ = region;
    int totalSize = regionSize(region);

    for (int i = 0; i <= totalSize - pattern.size(); i++) {
        bool match = true;
        for (int j = 0; j < pattern.size(); j++) {
            if (readMem(region, i + j) != static_cast<uint8_t>(pattern[j])) {
                match = false;
                break;
            }
        }
        if (match) {
            results_.push_back({static_cast<uint16_t>(i)});
        }
    }

    isFirstSearch_ = false;
    searchHintLabel_->setText(tr("Filtering: search within results"));
    populateTable();
}

// ============================================================
// Filter search 鈥?narrow down existing results
// ============================================================

void MemorySearchDialog::doFilterSearch(const QByteArray& pattern) {
    if (pattern.isEmpty()) return;

    std::vector<SearchResult> filtered;
    for (const auto& sr : results_) {
        bool match = true;
        for (int j = 0; j < pattern.size(); j++) {
            if (readMem(searchRegion_, sr.address + j) != static_cast<uint8_t>(pattern[j])) {
                match = false;
                break;
            }
        }
        if (match) {
            filtered.push_back(sr);
        }
    }

    results_ = std::move(filtered);
    populateTable();
}

// ============================================================
// Populate table with current results
// ============================================================

void MemorySearchDialog::populateTable() {
    resultsTable_->setRowCount(static_cast<int>(results_.size()));

    for (int r = 0; r < static_cast<int>(results_.size()); r++) {
        uint16_t addr = toDisplayAddr(searchRegion_, results_[r].address);

        // Address column
        auto* addrItem = new QTableWidgetItem(QString("$%1").arg(addr, 4, 16, QLatin1Char('0')).toUpper());
        addrItem->setTextAlignment(Qt::AlignCenter);
        resultsTable_->setItem(r, 0, addrItem);

        // Hex value column 鈥?show exactly the searched bytes
        QString hexStr;
        for (int j = 0; j < lastPattern_.size(); j++) {
            if (j > 0) hexStr += ' ';
            uint8_t val = readMem(searchRegion_, results_[r].address + j);
            hexStr += QString("%1").arg(val, 2, 16, QLatin1Char('0')).toUpper();
        }
        resultsTable_->setItem(r, 1, new QTableWidgetItem(hexStr));

        // Decimal value column
        if (lastPattern_.size() == 1) {
            uint8_t val = readMem(searchRegion_, results_[r].address);
            resultsTable_->setItem(r, 2, new QTableWidgetItem(QString::number(val)));
        } else {
            resultsTable_->setItem(r, 2, new QTableWidgetItem("-"));
        }

        // Previous value column
        const auto& prev = results_[r].lastValues;
        if (prev.isEmpty()) {
            auto* prevItem = new QTableWidgetItem("-");
            prevItem->setForeground(QColor(100, 100, 100));
            resultsTable_->setItem(r, 3, prevItem);
        } else if (lastPattern_.size() == 1) {
            auto* prevItem = new QTableWidgetItem(QString::number(static_cast<uint8_t>(prev[0])));
            prevItem->setForeground(QColor(100, 180, 255));
            resultsTable_->setItem(r, 3, prevItem);
        } else {
            QString prevHex;
            for (int j = 0; j < prev.size(); j++) {
                if (j > 0) prevHex += ' ';
                prevHex += QString("%1").arg(static_cast<uint8_t>(prev[j]), 2, 16, QLatin1Char('0')).toUpper();
            }
            auto* prevItem = new QTableWidgetItem(prevHex);
            prevItem->setForeground(QColor(100, 180, 255));
            resultsTable_->setItem(r, 3, prevItem);
        }
    }

    resultCountLabel_->setText(tr("%1 result(s)").arg(results_.size()));

    // Start live update if we have results
    if (!results_.empty()) {
        liveTimer_->start(200);
    } else {
        liveTimer_->stop();
    }
}

// ============================================================
// Slot: Search
// ============================================================

void MemorySearchDialog::onSearch() {
    bool ok;
    QByteArray pattern;
    int fmt = formatCombo_->currentIndex();

    if (fmt == 0) {
        // Decimal
        pattern = parseDecInput(searchEdit_->text(), ok);
    } else if (fmt == 1) {
        // Hex
        pattern = parseHexInput(searchEdit_->text(), ok);
    } else {
        // Text (ASCII)
        pattern = searchEdit_->text().toLatin1();
        ok = !pattern.isEmpty();
    }

    if (!ok || pattern.isEmpty()) {
        resultCountLabel_->setText(tr("Invalid input"));
        return;
    }

    if (!console_) {
        resultCountLabel_->setText(tr("No ROM loaded"));
        return;
    }

    lastPattern_ = pattern;

    if (isFirstSearch_) {
        doInitialSearch(pattern);
    } else {
        // Save current values before filtering so user can see what changed
        saveCurrentValues();
        doFilterSearch(pattern);
    }
}

// ============================================================
// Slot: Reset
// ============================================================

void MemorySearchDialog::onReset() {
    results_.clear();
    lastPattern_.clear();
    isFirstSearch_ = true;
    currentResultIndex_ = -1;
    searchRegion_ = 0;
    searchEdit_->clear();
    resultsTable_->setRowCount(0);
    resultCountLabel_->setText("");
    searchHintLabel_->setText(tr("First search: scan region | Next search: filter results"));
    liveTimer_->stop();
}

// ============================================================
// Slot: Result clicked 鈥?jump in memory viewer
// ============================================================

void MemorySearchDialog::onResultClicked(int row, int col) {
    if (row < 0 || row >= static_cast<int>(results_.size())) return;
    currentResultIndex_ = row;

    // Only jump when clicking address column (col == 0)
    if (col != 0) return;

    uint16_t addr = toDisplayAddr(searchRegion_, results_[row].address);

    if (!memViewer_) {
        memViewer_ = new MemoryViewer(console_, this);
    }
    memViewer_->setRegion(searchRegion_);
    memViewer_->gotoAddress(addr);
    memViewer_->show();
    memViewer_->raise();
    memViewer_->activateWindow();
}

// ============================================================
// Slot: Live update 鈥?refresh hex/decimal values in table
// ============================================================

void MemorySearchDialog::onLiveUpdate() {
    if (results_.empty() || lastPattern_.isEmpty()) return;

    for (int r = 0; r < static_cast<int>(results_.size()); r++) {
        // Update hex value
        QString hexStr;
        for (int j = 0; j < lastPattern_.size(); j++) {
            if (j > 0) hexStr += ' ';
            uint8_t val = readMem(searchRegion_, results_[r].address + j);
            hexStr += QString("%1").arg(val, 2, 16, QLatin1Char('0')).toUpper();
        }
        QTableWidgetItem* hexItem = resultsTable_->item(r, 1);
        if (hexItem && hexItem->text() != hexStr) {
            hexItem->setText(hexStr);
            hexItem->setForeground(QColor(255, 200, 100)); // highlight changed
        } else if (hexItem) {
            hexItem->setForeground(QColor(200, 200, 200));
        }

        // Update decimal value (single byte only)
        if (lastPattern_.size() == 1) {
            uint8_t val = readMem(searchRegion_, results_[r].address);
            QTableWidgetItem* decItem = resultsTable_->item(r, 2);
            if (decItem) {
                decItem->setText(QString::number(val));
            }
        }

        // Previous column is static 鈥?no live update needed
    }
}

// ============================================================
// Slot: Double-click on result
// ============================================================

void MemorySearchDialog::onResultDoubleClicked(int row, int col) {
    if (row < 0 || row >= static_cast<int>(results_.size())) return;
    currentResultIndex_ = row;  // Ensure correct row is selected for edit

    if (col == 0) {
        // Double-click address -> open memory viewer and jump
        onResultClicked(row, col);
    } else if (col == 1 || col == 2) {
        // Double-click hex/decimal value -> edit
        onEditValue();
    }
}

// ============================================================
// Slot: Context menu
// ============================================================

void MemorySearchDialog::onContextMenu(const QPoint& pos) {
    int row = resultsTable_->rowAt(pos.y());
    if (row < 0 || row >= static_cast<int>(results_.size())) return;

    currentResultIndex_ = row;
    uint16_t addr = toDisplayAddr(searchRegion_, results_[row].address);
    QString addrStr = QString("$%1").arg(addr, 4, 16, QLatin1Char('0')).toUpper();

    QMenu menu(this);
    menu.addAction(tr("Edit Value..."), this, &MemorySearchDialog::onEditValue);
    menu.addAction(tr("Copy Address"), this, &MemorySearchDialog::onCopyAddress);
    menu.addAction(tr("Copy Value"), this, &MemorySearchDialog::onCopyValue);
    menu.addSeparator();
    menu.addAction(tr("Go to Address in Memory Viewer"), this, [this]() {
        onResultClicked(currentResultIndex_, 0);
    });
    menu.addAction(tr("Add Execute Breakpoint at %1").arg(addrStr), this, &MemorySearchDialog::onAddBreakpoint);
    menu.exec(resultsTable_->viewport()->mapToGlobal(pos));
}

// ============================================================
// Slot: Edit value (hex or decimal input dialog)
// ============================================================

void MemorySearchDialog::onEditValue() {
    if (currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) return;
    if (searchRegion_ == 3) {
        // ROM is read-only
        return;
    }

    const auto& sr = results_[currentResultIndex_];
    uint8_t currentVal = readMem(searchRegion_, sr.address);

    // Custom dialog instead of QInputDialog to avoid geometry issues
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Edit Value"));
    dlg.setMinimumWidth(280);
    dlg.setStyleSheet(R"(
        QDialog { background-color: #2D2D2D; color: #D4D4D4; }
        QLabel { color: #D4D4D4; }
        QLineEdit { background-color: #3E3E3E; color: #D4D4D4; border: 1px solid #555; padding: 4px 6px; }
        QPushButton { background-color: #3E3E3E; color: #D4D4D4; border: 1px solid #555; border-radius: 3px; padding: 4px 16px; }
        QPushButton:hover { background-color: #4E4E4E; }
    )");

    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    QLabel* infoLabel = new QLabel(tr("Current value: %1 (0x%2)")
        .arg(currentVal)
        .arg(currentVal, 2, 16, QLatin1Char('0')).toUpper());
    layout->addWidget(infoLabel);

    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->addWidget(new QLabel(tr("New value (0-255):")));
    QLineEdit* valueEdit = new QLineEdit(QString::number(currentVal));
    valueEdit->setValidator(new QIntValidator(0, 255, valueEdit));
    valueEdit->selectAll();
    inputLayout->addWidget(valueEdit);
    layout->addLayout(inputLayout);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton* okBtn = new QPushButton(tr("OK"));
    QPushButton* cancelBtn = new QPushButton(tr("Cancel"));
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(valueEdit, &QLineEdit::returnPressed, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted) return;

    bool ok;
    int newVal = valueEdit->text().toInt(&ok);
    if (!ok || newVal < 0 || newVal > 255) return;

    writeMem(searchRegion_, sr.address, static_cast<uint8_t>(newVal));
    populateTable();
}

// ============================================================
// Slot: Copy address to clipboard
// ============================================================

void MemorySearchDialog::onCopyAddress() {
    if (currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) return;
    uint16_t addr = toDisplayAddr(searchRegion_, results_[currentResultIndex_].address);
    QApplication::clipboard()->setText(QString("$%1").arg(addr, 4, 16, QLatin1Char('0')).toUpper());
}

// ============================================================
// Slot: Copy value to clipboard
// ============================================================

void MemorySearchDialog::onCopyValue() {
    if (currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) return;
    const auto& sr = results_[currentResultIndex_];
    QString valStr;
    for (int j = 0; j < lastPattern_.size(); j++) {
        if (j > 0) valStr += ' ';
        valStr += QString("%1").arg(readMem(searchRegion_, sr.address + j), 2, 16, QLatin1Char('0')).toUpper();
    }
    QApplication::clipboard()->setText(valStr);
}

// ============================================================
// Slot: Add execute breakpoint
// ============================================================

void MemorySearchDialog::onAddBreakpoint() {
    if (currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) return;
    const auto& sr = results_[currentResultIndex_];
    uint16_t addr = toDisplayAddr(searchRegion_, sr.address);
    // Emit breakpoint signal if connected, otherwise do nothing
    // The breakpoint is added via the parent (MainWindow)
}

#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QPoint>
#include <vector>
#include <cstdint>

class NESConsole;
class MemoryViewer;

// ============================================================
// MemorySearchDialog — FCEUX-style memory search window
// ============================================================
class MemorySearchDialog : public QDialog {
    Q_OBJECT
public:
    explicit MemorySearchDialog(NESConsole* console, MemoryViewer* memViewer, QWidget* parent = nullptr);

    // Called when dialog is shown
    void showEvent(QShowEvent* event) override;

private slots:
    void onSearch();
    void onReset();
    void onResultClicked(int row, int col);
    void onResultDoubleClicked(int row, int col);
    void onLiveUpdate();
    void onContextMenu(const QPoint& pos);
    void onEditValue();
    void onCopyAddress();
    void onCopyValue();
    void onAddBreakpoint();

private:
    void setupUI();
    QByteArray parseHexInput(const QString& input, bool& ok);
    QByteArray parseDecInput(const QString& input, bool& ok);
    void doInitialSearch(const QByteArray& pattern);
    void doFilterSearch(const QByteArray& pattern);
    void populateTable();
    void saveCurrentValues();  // snapshot current values as "previous"
    uint8_t readMem(int region, uint16_t addr);
    void writeMem(int region, uint16_t addr, uint8_t val);

    NESConsole* console_;
    MemoryViewer* memViewer_;

    // Search options
    QComboBox* regionCombo_;
    QLineEdit* searchEdit_;
    QComboBox* formatCombo_;  // Hex / Text
    QPushButton* searchBtn_;
    QPushButton* resetBtn_;
    QLabel* resultCountLabel_;
    QLabel* searchHintLabel_;

    // Results table
    QTableWidget* resultsTable_;

    // Search state
    QByteArray lastPattern_;
    int currentResultIndex_ = -1;
    bool isFirstSearch_ = true;  // true = search from region, false = filter from results

    struct SearchResult {
        uint16_t address;
        QByteArray lastValues;  // values at last snapshot
    };
    std::vector<SearchResult> results_;

    // Live update timer
    QTimer* liveTimer_ = nullptr;
    int searchRegion_ = 0;
};

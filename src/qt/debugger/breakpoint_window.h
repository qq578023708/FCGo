#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QTimer>
#include <cstdint>

class BreakpointManager;

class BreakpointWindow : public QDialog {
    Q_OBJECT
public:
    explicit BreakpointWindow(BreakpointManager* bpm, QWidget* parent = nullptr);

    void refreshList();

private slots:
    void onToggleEnabled();
    void onDelete();
    void onClearAll();
    void onLiveUpdate();

private:
    void setupUI();
    QString typeString(uint16_t flags) const;

    BreakpointManager* bpm_;
    QTableWidget* table_;
    QTimer* liveTimer_ = nullptr;
};

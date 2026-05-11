#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QImage>
#include <QLabel>
#include <QTimer>
#include <memory>

class NESConsole;

// Pattern Table Viewer - shows CHR ROM/RAM tiles
class PatternTableWidget : public QWidget {
    Q_OBJECT
public:
    explicit PatternTableWidget(NESConsole* console, int tableIndex, QWidget* parent = nullptr);
    void refresh();

private:
    NESConsole* console_;
    int tableIndex_;  // 0 or 1
    QLabel* imageLabel_;
    QImage image_;
};

// Name Table Viewer - shows background rendering
class NameTableWidget : public QWidget {
    Q_OBJECT
public:
    explicit NameTableWidget(NESConsole* console, QWidget* parent = nullptr);
    void refresh();

private:
    NESConsole* console_;
    QLabel* imageLabel_;
    QImage image_;
};

// OAM Viewer - shows sprite data
class OAMWidget : public QWidget {
    Q_OBJECT
public:
    explicit OAMWidget(NESConsole* console, QWidget* parent = nullptr);
    void refresh();

private:
    NESConsole* console_;
    class QTableWidget* table_;
};

// Main PPU Viewer window with tabs
class PPUViewer : public QDialog {
    Q_OBJECT
public:
    explicit PPUViewer(NESConsole* console, QWidget* parent = nullptr);

private slots:
    void onRefresh();

private:
    NESConsole* console_;
    QTabWidget* tabs_;
    PatternTableWidget* pattern0_;
    PatternTableWidget* pattern1_;
    NameTableWidget* nameTable_;
    OAMWidget* oamWidget_;
    QTimer* refreshTimer_;
};

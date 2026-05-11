#include "breakpoint_window.h"
#include "../../core/debug/breakpoint.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>

BreakpointWindow::BreakpointWindow(BreakpointManager* bpm, QWidget* parent)
    : QDialog(parent)
    , bpm_(bpm)
{
    setupUI();
    setWindowTitle(tr("Breakpoints"));
    setMinimumSize(520, 350);
}

void BreakpointWindow::setupUI() {
    setStyleSheet(R"(
        QDialog { background-color: #1E1E1E; color: #D4D4D4; }
        QLabel { color: #D4D4D4; }
        QTableWidget { background-color: #1E1E1E; color: #D4D4D4; gridline-color: #3E3E3E; border: 1px solid #3E3E3E; selection-background-color: #0078D4; }
        QHeaderView::section { background-color: #2D2D2D; color: #D4D4D4; border: 1px solid #3E3E3E; padding: 5px; font-weight: bold; }
        QPushButton { background-color: #3E3E3E; color: #D4D4D4; border: 1px solid #555; border-radius: 3px; padding: 4px 12px; }
        QPushButton:hover { background-color: #4E4E4E; }
    )");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Table
    table_ = new QTableWidget();
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({tr("Enabled"), tr("Address"), tr("Type"), tr("Hits"), tr("Description")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);

    QFont mono("Consolas", 9);
    mono.setStyleHint(QFont::Monospace);
    table_->setFont(mono);

    mainLayout->addWidget(table_, 1);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* toggleBtn = new QPushButton(tr("Toggle Enable"));
    QPushButton* deleteBtn = new QPushButton(tr("Delete"));
    QPushButton* clearBtn = new QPushButton(tr("Clear All"));
    btnLayout->addWidget(toggleBtn);
    btnLayout->addWidget(deleteBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(clearBtn);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(toggleBtn, &QPushButton::clicked, this, &BreakpointWindow::onToggleEnabled);
    connect(deleteBtn, &QPushButton::clicked, this, &BreakpointWindow::onDelete);
    connect(clearBtn, &QPushButton::clicked, this, &BreakpointWindow::onClearAll);

    // Live update timer (500ms)
    liveTimer_ = new QTimer(this);
    connect(liveTimer_, &QTimer::timeout, this, &BreakpointWindow::onLiveUpdate);
    liveTimer_->start(500);

    refreshList();
}

QString BreakpointWindow::typeString(uint16_t flags) const {
    QStringList types;
    if (flags & 0x08) types << "X";  // Exec
    if (flags & 0x02) types << "R";  // Read
    if (flags & 0x04) types << "W";  // Write
    return types.join("/");
}

void BreakpointWindow::refreshList() {
    if (!bpm_) return;

    table_->setRowCount(bpm_->count());
    for (int i = 0; i < bpm_->count(); i++) {
        const auto& bp = bpm_->at(i);

        // Enabled checkbox
        auto* enabledItem = new QTableWidgetItem(bp.isEnabled() ? "✓" : "✗");
        enabledItem->setTextAlignment(Qt::AlignCenter);
        enabledItem->setForeground(bp.isEnabled() ? QColor(100, 255, 100) : QColor(255, 100, 100));
        table_->setItem(i, 0, enabledItem);

        // Address
        QString addrStr = QString("$%1").arg(bp.address, 4, 16, QLatin1Char('0')).toUpper();
        if (bp.endAddress != 0) {
            addrStr += QString("-$%1").arg(bp.endAddress, 4, 16, QLatin1Char('0')).toUpper();
        }
        auto* addrItem = new QTableWidgetItem(addrStr);
        addrItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(i, 1, addrItem);

        // Type
        auto* typeItem = new QTableWidgetItem(typeString(bp.flags));
        typeItem->setTextAlignment(Qt::AlignCenter);
        if (bp.isExec()) typeItem->setForeground(QColor(100, 180, 255));
        else if (bp.isRead()) typeItem->setForeground(QColor(255, 200, 100));
        else if (bp.isWrite()) typeItem->setForeground(QColor(255, 100, 100));
        table_->setItem(i, 2, typeItem);

        // Hit count
        auto* hitItem = new QTableWidgetItem(QString::number(bp.hitCount));
        hitItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(i, 3, hitItem);

        // Description
        table_->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(bp.desc)));
    }
}

void BreakpointWindow::onToggleEnabled() {
    if (!bpm_) return;
    int row = table_->currentRow();
    if (row < 0 || row >= bpm_->count()) return;

    auto& bp = bpm_->at(row);
    if (bp.isEnabled()) {
        bp.flags &= ~0x01;  // Clear BP_ENABLE
    } else {
        bp.flags |= 0x01;   // Set BP_ENABLE
    }
    refreshList();
}

void BreakpointWindow::onDelete() {
    if (!bpm_) return;
    int row = table_->currentRow();
    if (row < 0 || row >= bpm_->count()) return;

    bpm_->remove(row);
    refreshList();
}

void BreakpointWindow::onClearAll() {
    if (!bpm_) return;
    bpm_->clear();
    refreshList();
}

void BreakpointWindow::onLiveUpdate() {
    // Update hit counts without full refresh
    if (!bpm_) return;
    for (int i = 0; i < bpm_->count() && i < table_->rowCount(); i++) {
        auto* hitItem = table_->item(i, 3);
        if (hitItem) {
            int count = bpm_->at(i).hitCount;
            QString newText = QString::number(count);
            if (hitItem->text() != newText) {
                hitItem->setText(newText);
                hitItem->setForeground(QColor(255, 200, 100)); // highlight change
            }
        }
    }
}

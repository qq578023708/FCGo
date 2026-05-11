#include "network_lobby_window.h"
#include "../../core/nes_console.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSplitter>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QMenu>
#include <QRegularExpression>

NetworkLobbyWindow::NetworkLobbyWindow(NESConsole* console, QWidget* parent)
    : QDialog(nullptr, Qt::Window)
    , console_(console)
{
    Q_UNUSED(parent);
    steam_ = new SteamManager(this);
    setupUI();
    setWindowTitle(tr("Network Lobby"));
    setMinimumSize(650, 500);

    if (steam_->init()) {
        playerLabel_->setText(tr("Player: %1").arg(steam_->getPlayerName()));
        statusLabel_->setText(tr("Connected to Steam"));
    } else {
        statusLabel_->setText(tr("Steam not running. Please start Steam first."));
        createBtn_->setEnabled(false);
        refreshBtn_->setEnabled(false);
        joinBtn_->setEnabled(false);
    }

    // Connections
    connect(steam_, &SteamManager::lobbyCreated, this, &NetworkLobbyWindow::onLobbyCreated);
    connect(steam_, &SteamManager::lobbyJoined, this, &NetworkLobbyWindow::onLobbyJoined);
    connect(steam_, &SteamManager::lobbyListUpdated, this, &NetworkLobbyWindow::onLobbyListUpdated);
    connect(steam_, &SteamManager::lobbyDataUpdated, this, [this](uint64_t) {
        updateMemberList();
        // Update host status text
        if (inLobby_) {
            int count = steam_->getLobbyMemberCount();
            statusLabel_->setText(tr("Lobby created. %1 player(s) connected.").arg(count));
        }
    });
    connect(steam_, &SteamManager::playerJoined, this, &NetworkLobbyWindow::onPlayerJoined);
    connect(steam_, &SteamManager::playerLeft, this, &NetworkLobbyWindow::onPlayerLeft);
    connect(steam_, &SteamManager::peerDisconnected, this, &NetworkLobbyWindow::onPeerDisconnected);
}

NetworkLobbyWindow::~NetworkLobbyWindow() {
    // Don't shutdown Steam here - it may be used elsewhere
}

void NetworkLobbyWindow::setupUI() {
    setStyleSheet(R"(
        QDialog { background-color: #1E1E1E; color: #D4D4D4; }
        QLabel { color: #D4D4D4; }
        QLineEdit { background-color: #3E3E3E; color: #D4D4D4; border: 1px solid #555; padding: 4px 6px; }
        QListWidget { background-color: #1E1E1E; color: #D4D4D4; border: 1px solid #3E3E3E; }
        QTableWidget { background-color: #1E1E1E; color: #D4D4D4; gridline-color: #3E3E3E; border: 1px solid #3E3E3E; selection-background-color: #0078D4; }
        QHeaderView::section { background-color: #2D2D2D; color: #D4D4D4; border: 1px solid #3E3E3E; padding: 5px; font-weight: bold; }
        QPushButton { background-color: #3E3E3E; color: #D4D4D4; border: 1px solid #555; border-radius: 3px; padding: 6px 16px; }
        QPushButton:hover { background-color: #4E4E4E; }
        QPushButton:disabled { background-color: #2D2D2D; color: #666; }
    )");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Top bar
    QHBoxLayout* topLayout = new QHBoxLayout();
    playerLabel_ = new QLabel(tr("Player: ---"));
    topLayout->addWidget(playerLabel_);
    topLayout->addStretch();
    statusLabel_ = new QLabel(tr("Initializing..."));
    topLayout->addWidget(statusLabel_);
    mainLayout->addLayout(topLayout);

    // Create lobby section
    QHBoxLayout* createLayout = new QHBoxLayout();
    createLayout->addWidget(new QLabel(tr("Lobby Name:")));
    lobbyNameEdit_ = new QLineEdit(tr("FCGo Game"));
    createLayout->addWidget(lobbyNameEdit_, 1);
    createBtn_ = new QPushButton(tr("Create Lobby"));
    createLayout->addWidget(createBtn_);
    refreshBtn_ = new QPushButton(tr("Refresh"));
    createLayout->addWidget(refreshBtn_);
    mainLayout->addLayout(createLayout);

    // Splitter: lobby list (left) + lobby info (right)
    QSplitter* splitter = new QSplitter(Qt::Horizontal);

    // Lobby list
    lobbyTable_ = new QTableWidget();
    lobbyTable_->setColumnCount(5);
    lobbyTable_->setHorizontalHeaderLabels({tr("Lobby Name"), tr("Host"), tr("Players"), tr("Ping"), tr("ROM")});
    lobbyTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    lobbyTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    lobbyTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    lobbyTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    lobbyTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    lobbyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    lobbyTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    lobbyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    lobbyTable_->verticalHeader()->setVisible(false);
    splitter->addWidget(lobbyTable_);

    // Right panel: members + chat
    QWidget* rightPanel = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    // Members
    rightLayout->addWidget(new QLabel(tr("Members:")));
    memberList_ = new QListWidget();
    memberList_->setMaximumHeight(100);
    rightLayout->addWidget(memberList_);

    // Chat
    rightLayout->addWidget(new QLabel(tr("Chat:")));
    chatList_ = new QListWidget();
    rightLayout->addWidget(chatList_, 1);

    QHBoxLayout* chatInputLayout = new QHBoxLayout();
    chatInput_ = new QLineEdit();
    chatInput_->setPlaceholderText(tr("Type message..."));
    QPushButton* sendBtn = new QPushButton(tr("Send"));
    chatInputLayout->addWidget(chatInput_, 1);
    chatInputLayout->addWidget(sendBtn);
    rightLayout->addLayout(chatInputLayout);

    splitter->addWidget(rightPanel);
    splitter->setSizes({350, 300});
    mainLayout->addWidget(splitter, 1);

    // Bottom buttons
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    joinBtn_ = new QPushButton(tr("Join Lobby"));
    leaveBtn_ = new QPushButton(tr("Leave Lobby"));
    leaveBtn_->setEnabled(false);
    bottomLayout->addWidget(joinBtn_);
    bottomLayout->addWidget(leaveBtn_);
    bottomLayout->addStretch();
    mainLayout->addLayout(bottomLayout);

    // Connections
    connect(createBtn_, &QPushButton::clicked, this, &NetworkLobbyWindow::onCreateLobby);
    connect(refreshBtn_, &QPushButton::clicked, this, &NetworkLobbyWindow::onRefreshLobbies);
    connect(joinBtn_, &QPushButton::clicked, this, &NetworkLobbyWindow::onJoinLobby);
    connect(leaveBtn_, &QPushButton::clicked, this, &NetworkLobbyWindow::onLeaveLobby);
    connect(lobbyTable_, &QTableWidget::cellClicked, this, &NetworkLobbyWindow::onLobbySelected);
    connect(chatInput_, &QLineEdit::returnPressed, this, &NetworkLobbyWindow::onSendChat);
    connect(sendBtn, &QPushButton::clicked, this, &NetworkLobbyWindow::onSendChat);

    // Member list context menu (kick player)
    memberList_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(memberList_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (!steam_->isHost()) return;
        QListWidgetItem* item = memberList_->itemAt(pos);
        if (!item) return;
        QString text = item->text();
        // Remove "[Host]" tag if present
        text.remove(tr(" [Host]"));
        if (text.isEmpty()) return;

        QMenu menu(this);
        QAction* kickAction = menu.addAction(tr("Kick player: %1").arg(text));
        if (menu.exec(memberList_->mapToGlobal(pos)) == kickAction) {
            // Find steamId by name from member list
            auto members = steam_->getLobbyMembers();
            for (const auto& m : members) {
                if (m.name == text) {
                    steam_->kickPlayer(m.steamId);
                    chatList_->addItem(tr(">>> Kicked player: %1").arg(text));
                    break;
                }
            }
        }
    });

    // Live update timer (poll Steam callbacks)
    liveTimer_ = new QTimer(this);
    connect(liveTimer_, &QTimer::timeout, this, &NetworkLobbyWindow::onLiveUpdate);
    liveTimer_->start(100); // 10 Hz
}

void NetworkLobbyWindow::onCreateLobby() {
    QString name = lobbyNameEdit_->text().trimmed();
    if (name.isEmpty()) name = tr("FCGo Game");
    statusLabel_->setText(tr("Creating lobby..."));
    steam_->createLobby(name, 2);
}

void NetworkLobbyWindow::onRefreshLobbies() {
    statusLabel_->setText(tr("Searching lobbies..."));
    lobbyTable_->setRowCount(0);
    steam_->searchLobbies();
}

void NetworkLobbyWindow::onJoinLobby() {
    if (selectedLobbyRow_ < 0) return;
    auto lobbies = steam_->getLobbyList();
    if (selectedLobbyRow_ >= static_cast<int>(lobbies.size())) return;

    statusLabel_->setText(tr("Joining lobby..."));
    steam_->joinLobby(lobbies[selectedLobbyRow_].lobbyId);
}

void NetworkLobbyWindow::onLeaveLobby() {
    steam_->leaveLobby();
    inLobby_ = false;
    romSynced_ = false;
    memberList_->clear();
    updateUIState();
    statusLabel_->setText(tr("Left lobby"));
    emit netPlayEnded(tr("Left lobby"));
}

void NetworkLobbyWindow::onLobbyCreated(const LobbyCreatedResult& result) {
    if (result.success) {
        inLobby_ = true;
        updateUIState();
        updateMemberList();
        statusLabel_->setText(tr("Lobby created. Waiting for players..."));
        chatList_->addItem(tr(">>> Lobby created. Share your lobby name for others to join."));
    } else {
        statusLabel_->setText(tr("Failed to create lobby: %1").arg(result.error));
    }
}

void NetworkLobbyWindow::onLobbyJoined(const LobbyJoinedResult& result) {
    if (result.success) {
        inLobby_ = true;
        updateUIState();
        updateMemberList();
        statusLabel_->setText(tr("Joined lobby!"));
        chatList_->addItem(tr(">>> Joined lobby."));
    } else {
        statusLabel_->setText(tr("Failed to join: %1").arg(result.error));
    }
}

void NetworkLobbyWindow::onLobbyListUpdated(const LobbyListResult& result) {
    if (!result.success) {
        statusLabel_->setText(tr("Failed to search lobbies"));
        return;
    }

    auto lobbies = steam_->getLobbyList();
    lobbyTable_->setRowCount(static_cast<int>(lobbies.size()));
    for (int i = 0; i < static_cast<int>(lobbies.size()); i++) {
        const auto& lobby = lobbies[i];
        lobbyTable_->setItem(i, 0, new QTableWidgetItem(lobby.name));
        lobbyTable_->setItem(i, 1, new QTableWidgetItem(lobby.hostName));
        lobbyTable_->setItem(i, 2, new QTableWidgetItem(QString("%1/%2").arg(lobby.playerCount).arg(lobby.maxPlayers)));

        // Ping with text and color
        QTableWidgetItem* pingItem = new QTableWidgetItem();
        pingItem->setTextAlignment(Qt::AlignCenter);
        if (lobby.ping < 0) {
            pingItem->setText(tr("N/A"));
            pingItem->setForeground(QColor("#888888"));
        } else if (lobby.ping <= 60) {
            pingItem->setText(QString("%1 ms").arg(lobby.ping));
            pingItem->setForeground(QColor("#4CAF50"));  // Green - excellent
        } else if (lobby.ping <= 100) {
            pingItem->setText(QString("%1 ms").arg(lobby.ping));
            pingItem->setForeground(QColor("#FFC107"));  // Yellow - good
        } else {
            pingItem->setText(QString("%1 ms").arg(lobby.ping));
            pingItem->setForeground(QColor("#F44336"));  // Red - poor
        }
        lobbyTable_->setItem(i, 3, pingItem);

        lobbyTable_->setItem(i, 4, new QTableWidgetItem(lobby.romName));
    }

    if (lobbies.empty()) {
        statusLabel_->setText(tr("No lobbies found"));
    } else {
        statusLabel_->setText(tr("Found %1 lobby(ies)").arg(lobbies.size()));
    }
}

void NetworkLobbyWindow::onPlayerJoined(uint64_t steamId, const QString& name) {
    chatList_->addItem(tr(">>> %1 joined the lobby").arg(name));
    updateMemberList();
    emit playerJoined(steamId, name);

    // If host has already loaded a ROM, re-send ROM info to the new client
    if (steam_->isHost() && !hostROMData_.isEmpty()) {
        sendROMInfo(hostROMFilename_);
    }
}

void NetworkLobbyWindow::onPlayerLeft(uint64_t steamId) {
    chatList_->addItem(tr(">>> A player left the lobby"));
    updateMemberList();
}

void NetworkLobbyWindow::onPeerDisconnected(uint64_t steamId) {
    chatList_->addItem(tr(">>> Peer disconnected. Network play ended."));
    statusLabel_->setText(tr("Peer disconnected"));
    emit netPlayEnded(tr("Peer disconnected"));
}

void NetworkLobbyWindow::onLobbySelected(int row) {
    selectedLobbyRow_ = row;
    joinBtn_->setEnabled(row >= 0 && !inLobby_);
}

void NetworkLobbyWindow::onSendChat() {
    QString msg = chatInput_->text().trimmed();
    if (msg.isEmpty()) return;
    chatList_->addItem(QString("%1: %2").arg(steam_->getPlayerName(), msg));
    chatInput_->clear();

    // Send to other players via P2P
    QByteArray data = ("CHAT:" + msg.toUtf8());
    steam_->sendNetData(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

void NetworkLobbyWindow::handleNetMessage(const QByteArray& data) {
    if (data.startsWith("CHAT:")) {
        QString msg = QString::fromUtf8(data.mid(5));
        chatList_->addItem(tr("Other: %1").arg(msg));
    }
    else if (data.startsWith("ROM_INFO|")) {
        handleROMInfo(data.mid(9));  // "ROM_INFO|" is 9 chars
    }
    else if (data.startsWith("ROM_CHUNK:")) {
        handleROMChunk(data.mid(10));
    }
    else if (data.startsWith("ROM_REQUEST")) {
        // Client requests ROM transfer, host starts sending chunks
        if (steam_->isHost()) {
            hostROMChunkIndex_ = 0;
            sendROMChunk(0);
        }
    }
    else if (data.startsWith("ROM_NEXT_CHUNK")) {
        // Client requests next chunk
        if (steam_->isHost()) {
            sendROMChunk(hostROMChunkIndex_);
        }
    }
    else if (data.startsWith("ROM_COMPLETE")) {
        // Transfer complete (client side)
        if (!steam_->isHost() && clientROMReceivedChunks_ > 0) {
            // Verify SHA256
            QByteArray actualHash = QCryptographicHash::hash(clientROMBuffer_, QCryptographicHash::Sha256).toHex();
            if (QString::fromUtf8(actualHash) == clientROMSHA256_) {
                // Save to persistent cache
                QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/rom_cache";
                QDir().mkpath(cacheDir);
                QString cachedPath = cacheDir + "/" + clientROMFilename_;
                QFile cachedFile(cachedPath);
                if (cachedFile.open(QIODevice::WriteOnly)) {
                    cachedFile.write(clientROMBuffer_);
                    cachedFile.close();
                    syncedROMPath_ = cachedPath;
                } else {
                    // Fallback to temp file
                    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
                    QString tempPath = tempDir + "/fcgo_net_" + clientROMFilename_;
                    QFile tempFile(tempPath);
                    if (tempFile.open(QIODevice::WriteOnly)) {
                        tempFile.write(clientROMBuffer_);
                        tempFile.close();
                        syncedROMPath_ = tempPath;
                    }
                }
                romSynced_ = true;
                statusLabel_->setText(tr("ROM synced: %1").arg(clientROMFilename_));
                chatList_->addItem(tr(">>> ROM transfer complete!"));
                steam_->sendNetMessage("ROM_SYNC_OK");
                emit romSyncComplete(syncedROMPath_);
            } else {
                statusLabel_->setText(tr("ROM transfer failed: checksum mismatch"));
                chatList_->addItem(tr(">>> ROM checksum mismatch!"));
            }
        }
    }
    else if (data.startsWith("ROM_LOCAL_OK") || data.startsWith("ROM_SYNC_OK")) {
        // Host receives confirmation that client has the ROM
        if (steam_->isHost()) {
            statusLabel_->setText(tr("All clients have the ROM. Ready to start!"));
            chatList_->addItem(tr(">>> Client ROM sync complete."));
        }
    }
}

void NetworkLobbyWindow::onLiveUpdate() {
    steam_->runCallbacks();

    // When not in active net play, handle network messages here
    // (ROM sync, chat, etc.). During net play, all reception is in
    // MainWindow::onFrameTick to avoid race conditions.
    if (!romSynced_ && inLobby_) {
        uint8_t buffer[8192];
        size_t len = sizeof(buffer);
        uint64_t senderId = 0;
        while (steam_->receiveNetData(buffer, &len, &senderId)) {
            QByteArray data(reinterpret_cast<const char*>(buffer), static_cast<int>(len));
            handleNetMessage(data);
        }
    }

    // Update latency display in member list
    if (inLobby_ && steam_->getLatency() > 0) {
        updateMemberListWithLatency();
    }

    // Auto-refresh lobby list every 5 seconds when not in a lobby
    if (!inLobby_) {
        lobbyRefreshCounter_++;
        if (lobbyRefreshCounter_ >= 50) {  // 50 * 100ms = 5 seconds
            lobbyRefreshCounter_ = 0;
            steam_->searchLobbies();
        }
    }
}

void NetworkLobbyWindow::updateMemberList() {
    memberList_->clear();
    auto members = steam_->getLobbyMembers();
    for (const auto& m : members) {
        QString tag = m.isHost ? tr(" [Host]") : "";
        memberList_->addItem(QString("%1%2").arg(m.name, tag));
    }
}

void NetworkLobbyWindow::updateMemberListWithLatency() {
    int latency = steam_->getLatency();
    if (latency <= 0) return;

    // Update the first non-host member (the peer) with latency
    for (int i = 0; i < memberList_->count(); ++i) {
        QListWidgetItem* item = memberList_->item(i);
        QString text = item->text();
        if (!text.contains(tr("[Host]"))) {
            // Remove old latency if present
            text.remove(QRegularExpression("\\s*\\(\\d+ms\\)"));
            item->setText(QString("%1 (%2ms)").arg(text).arg(latency));
        }
    }
}

void NetworkLobbyWindow::updateUIState() {
    createBtn_->setEnabled(!inLobby_);
    refreshBtn_->setEnabled(!inLobby_);
    joinBtn_->setEnabled(!inLobby_ && selectedLobbyRow_ >= 0);
    leaveBtn_->setEnabled(inLobby_);
    lobbyTable_->setEnabled(!inLobby_);
}

// ============================================================
// ROM Sync - Host side
// ============================================================

void NetworkLobbyWindow::hostLoadROM(const QString& romPath) {
    if (!inLobby_) return;

    QFile file(romPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "hostLoadROM: failed to open" << romPath;
        return;
    }

    hostROMData_ = file.readAll();
    file.close();

    if (hostROMData_.isEmpty()) {
        qWarning() << "hostLoadROM: empty ROM data";
        return;
    }

    hostROMFilename_ = QFileInfo(romPath).fileName();
    hostROMSHA256_ = QString::fromUtf8(
        QCryptographicHash::hash(hostROMData_, QCryptographicHash::Sha256).toHex());

    qDebug() << "hostLoadROM:" << hostROMFilename_
             << "size:" << hostROMData_.size()
             << "sha256:" << hostROMSHA256_
             << "isHost:" << steam_->isHost();

    // Update lobby data so search results show the ROM name
    steam_->setLobbyData("rom", hostROMFilename_);

    // Send ROM info to clients
    sendROMInfo(romPath);

    chatList_->addItem(tr(">>> ROM loaded: %1 (%2 KB)").arg(hostROMFilename_).arg(hostROMData_.size() / 1024));
    statusLabel_->setText(tr("ROM loaded: %1. Waiting for clients...").arg(hostROMFilename_));

    // Update lobby metadata so other players can see ROM in lobby list
    steam_->setLobbyData("rom", hostROMFilename_);
}

void NetworkLobbyWindow::sendROMInfo(const QString& romPath) {
    // Format: ROM_INFO|filename|size|sha256
    QByteArray msg = "ROM_INFO|" + hostROMFilename_.toUtf8() + "|" +
                    QByteArray::number(hostROMData_.size()) + "|" +
                    hostROMSHA256_.toUtf8();
    steam_->sendNetMessage(msg);
}

void NetworkLobbyWindow::sendROMChunk(int chunkIndex) {
    int totalChunks = (hostROMData_.size() + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if (chunkIndex >= totalChunks) {
        // All chunks sent - send complete signal
        steam_->sendNetMessage("ROM_COMPLETE");
        qDebug() << "NetworkLobbyWindow: ROM transfer complete," << totalChunks << "chunks";
        return;
    }

    int offset = chunkIndex * CHUNK_SIZE;
    int size = qMin(CHUNK_SIZE, hostROMData_.size() - offset);
    QByteArray chunkData = hostROMData_.mid(offset, size);

    // Format: index:total:base64data
    QByteArray msg = "ROM_CHUNK:" +
                    QByteArray::number(chunkIndex) + ":" +
                    QByteArray::number(totalChunks) + ":" +
                    chunkData.toBase64();
    steam_->sendNetMessage(msg);

    hostROMChunkIndex_ = chunkIndex + 1;
}

// ============================================================
// ROM Sync - Client side
// ============================================================

void NetworkLobbyWindow::handleROMInfo(const QByteArray& payload) {
    // Parse: filename|size|sha256
    QList<QByteArray> parts = payload.split('|');
    qDebug() << "handleROMInfo: payload size =" << payload.size() << "parts =" << parts.size();
    if (parts.size() >= 1) qDebug() << "  filename:" << parts[0];
    if (parts.size() >= 2) qDebug() << "  size:" << parts[1];
    if (parts.size() >= 3) qDebug() << "  sha256:" << parts[2];

    if (parts.size() < 3) {
        qWarning() << "handleROMInfo: invalid payload, parts =" << parts.size();
        return;
    }

    clientROMFilename_ = QString::fromUtf8(parts[0]);
    int fileSize = parts[1].toInt();
    clientROMSHA256_ = QString::fromUtf8(parts[2]);

    chatList_->addItem(tr(">>> Host loaded ROM: %1 (%2 KB)").arg(clientROMFilename_).arg(fileSize / 1024));
    statusLabel_->setText(tr("Host loaded ROM: %1").arg(clientROMFilename_));

    // Reset sync state for new ROM
    romSynced_ = false;
    syncedROMPath_.clear();

    // Reset receive state
    clientROMBuffer_.clear();
    clientROMBuffer_.resize(fileSize);
    clientROMTotalChunks_ = 0;
    clientROMReceivedChunks_ = 0;

    // Try to find the ROM locally
    QString localPath = tryFindLocalROM(clientROMFilename_, clientROMSHA256_);
    if (!localPath.isEmpty()) {
        syncedROMPath_ = localPath;
        romSynced_ = true;
        statusLabel_->setText(tr("ROM found locally: %1").arg(clientROMFilename_));
        chatList_->addItem(tr(">>> ROM found locally, no transfer needed"));
        steam_->sendNetMessage("ROM_LOCAL_OK");
        emit romSyncComplete(localPath);
        return;
    }

    // Request ROM transfer from host
    requestROMTransfer();
}

void NetworkLobbyWindow::requestROMTransfer() {
    statusLabel_->setText(tr("Requesting ROM transfer..."));
    steam_->sendNetMessage("ROM_REQUEST");
}

void NetworkLobbyWindow::handleROMChunk(const QByteArray& payload) {
    // Parse: index:total:base64data
    int colon1 = payload.indexOf(':');
    int colon2 = payload.indexOf(':', colon1 + 1);
    if (colon1 < 0 || colon2 < 0) return;

    int chunkIndex = payload.mid(0, colon1).toInt();
    int totalChunks = payload.mid(colon1 + 1, colon2 - colon1 - 1).toInt();
    QByteArray chunkData = QByteArray::fromBase64(payload.mid(colon2 + 1));

    if (clientROMTotalChunks_ == 0) {
        clientROMTotalChunks_ = totalChunks;
    }

    // Copy chunk data into buffer
    int offset = chunkIndex * CHUNK_SIZE;
    if (offset + chunkData.size() <= clientROMBuffer_.size()) {
        memcpy(clientROMBuffer_.data() + offset, chunkData.constData(), chunkData.size());
    }

    clientROMReceivedChunks_++;

    // Update progress
    int percent = (clientROMReceivedChunks_ * 100) / clientROMTotalChunks_;
    statusLabel_->setText(tr("Receiving ROM: %1%").arg(percent));

    // Send next chunk request (flow control: request one at a time)
    // Always request next to trigger host's ROM_COMPLETE when all chunks are done
    steam_->sendNetMessage("ROM_NEXT_CHUNK");

    // Check if all chunks received (handled by ROM_COMPLETE message)
}

QString NetworkLobbyWindow::tryFindLocalROM(const QString& filename, const QString& sha256) {
    // 1. Check persistent ROM cache first
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/rom_cache";
    QDir().mkpath(cacheDir);
    QString cachedPath = cacheDir + "/" + filename;
    if (QFile::exists(cachedPath)) {
        QByteArray hash = computeFileSHA256(cachedPath);
        if (QString::fromUtf8(hash.toHex()) == sha256) {
            return cachedPath;
        }
    }

    // 2. Search in common ROM directories
    QStringList searchDirs = {
        QCoreApplication::applicationDirPath(),
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
    };

    for (const auto& dir : searchDirs) {
        QString path = dir + "/" + filename;
        if (QFile::exists(path)) {
            QByteArray hash = computeFileSHA256(path);
            if (QString::fromUtf8(hash.toHex()) == sha256) {
                return path;
            }
        }
    }
    return {};
}

QByteArray NetworkLobbyWindow::computeFileSHA256(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        hash.addData(file.read(8192));
    }
    return hash.result();
}
